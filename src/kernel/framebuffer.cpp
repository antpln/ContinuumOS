#include <kernel/framebuffer.h>

#include <kernel/debug.h>
#include <kernel/port_io.h>

#include <string.h>

namespace framebuffer
{
namespace
{
FrameBufferInfo g_info{};
bool g_available = false;
uint32_t g_bytes_per_pixel = 0;
uint32_t g_physical_address_low = 0;
uint32_t g_framebuffer_size = 0;

struct [[gnu::packed]] VbeModeInfo
{
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t real_fct_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved1;
    uint8_t red_mask_size;
    uint8_t red_position;
    uint8_t green_mask_size;
    uint8_t green_position;
    uint8_t blue_mask_size;
    uint8_t blue_position;
    uint8_t reserved_mask_size;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    uint32_t phys_base_ptr;
    uint32_t reserved2;
    uint16_t reserved3;
    uint8_t reserved4[206];
};

constexpr uint16_t VBE_DISPI_IOPORT_INDEX = 0x01CE;
constexpr uint16_t VBE_DISPI_IOPORT_DATA = 0x01CF;
constexpr uint16_t VBE_DISPI_INDEX_ID = 0x00;
constexpr uint16_t VBE_DISPI_INDEX_XRES = 0x01;
constexpr uint16_t VBE_DISPI_INDEX_YRES = 0x02;
constexpr uint16_t VBE_DISPI_INDEX_BPP = 0x03;
constexpr uint16_t VBE_DISPI_INDEX_ENABLE = 0x04;
constexpr uint16_t VBE_DISPI_INDEX_VIRT_WIDTH = 0x05;
constexpr uint16_t VBE_DISPI_INDEX_VIRT_HEIGHT = 0x06;
constexpr uint16_t VBE_DISPI_INDEX_X_OFFSET = 0x08;
constexpr uint16_t VBE_DISPI_INDEX_Y_OFFSET = 0x09;

constexpr uint16_t VBE_DISPI_DISABLED = 0x00;
constexpr uint16_t VBE_DISPI_ENABLED = 0x01;
constexpr uint16_t VBE_DISPI_LFB_ENABLED = 0x40;

constexpr uint16_t VBE_DISPI_ID0 = 0xB0C0;
constexpr uint16_t VBE_DISPI_ID5 = 0xB0C5;

constexpr uintptr_t BOCHS_FRAMEBUFFER_PHYS = 0xE0000000;

constexpr uint16_t PCI_CONFIG_ADDRESS = 0x0CF8;
constexpr uint16_t PCI_CONFIG_DATA = 0x0CFC;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    const uint32_t address = (1u << 31) |
                             (static_cast<uint32_t>(bus) << 16) |
                             (static_cast<uint32_t>(device) << 11) |
                             (static_cast<uint32_t>(function) << 8) |
                             (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    const uint32_t address = (1u << 31) |
                             (static_cast<uint32_t>(bus) << 16) |
                             (static_cast<uint32_t>(device) << 11) |
                             (static_cast<uint32_t>(function) << 8) |
                             (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

bool find_pci_vga_framebuffer(uintptr_t &out_address)
{
    for (uint16_t bus = 0; bus < 256; ++bus)
    {
        for (uint8_t device = 0; device < 32; ++device)
        {
            const uint8_t bus_id = static_cast<uint8_t>(bus);
            const uint32_t vendor_device = pci_config_read_dword(bus_id, device, 0, 0x00);
            if (vendor_device == 0xFFFFFFFFu)
            {
                continue;
            }

            const uint32_t header_type_reg = pci_config_read_dword(bus_id, device, 0, 0x0C);
            const bool multifunction = (header_type_reg & 0x00800000u) != 0;
            const uint8_t function_limit = multifunction ? 8 : 1;

            for (uint8_t function = 0; function < function_limit; ++function)
            {
                const uint32_t vendor = pci_config_read_dword(bus_id, device, function, 0x00);
                if (vendor == 0xFFFFFFFFu)
                {
                    continue;
                }

                const uint32_t class_reg = pci_config_read_dword(bus_id, device, function, 0x08);
                const uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
                if (base_class != 0x03)
                {
                    continue;
                }

                const uint8_t header_type = static_cast<uint8_t>((pci_config_read_dword(bus_id, device, function, 0x0C) >> 16) & 0x7F);
                if (header_type != 0x00)
                {
                    continue;
                }

                for (uint8_t bar_index = 0; bar_index < 6; ++bar_index)
                {
                    const uint8_t offset = static_cast<uint8_t>(0x10 + bar_index * 4);
                    uint32_t bar = pci_config_read_dword(bus_id, device, function, offset);
                    if (bar == 0 || (bar & 0x1u) != 0)
                    {
                        continue;
                    }

                    const uint32_t memory_type = (bar >> 1) & 0x3u;
                    const bool prefetchable = (bar & 0x8u) != 0;
                    uint64_t base = bar & 0xFFFFFFF0u;
                    const uint8_t current_bar = bar_index;

                    uint64_t size_mask = 0;

                    pci_config_write_dword(bus_id, device, function, offset, 0xFFFFFFFFu);
                    const uint32_t size_low = pci_config_read_dword(bus_id, device, function, offset) & 0xFFFFFFF0u;
                    pci_config_write_dword(bus_id, device, function, offset, bar);
                    size_mask |= size_low;

                    if (memory_type == 0x2u && bar_index + 1 < 6)
                    {
                        const uint8_t upper_offset = static_cast<uint8_t>(offset + 4);
                        const uint32_t orig_upper = pci_config_read_dword(bus_id, device, function, upper_offset);
                        pci_config_write_dword(bus_id, device, function, upper_offset, 0xFFFFFFFFu);
                        const uint32_t size_high = pci_config_read_dword(bus_id, device, function, upper_offset);
                        pci_config_write_dword(bus_id, device, function, upper_offset, orig_upper);
                        base |= static_cast<uint64_t>(orig_upper) << 32;
                        size_mask |= static_cast<uint64_t>(size_high) << 32;
                        ++bar_index;
                    }

                    if (base == 0 || size_mask == 0)
                    {
                        continue;
                    }

                    const uint64_t size = (~size_mask + 1);
                    if ((!prefetchable && size < 0x00400000ULL) || size < 0x00100000ULL)
                    {
                        continue;
                    }

                    if (base < 0x01000000u)
                    {
                        continue;
                    }

                    out_address = static_cast<uintptr_t>(base);
                    debug("[FB] PCI VGA %02x:%02x.%u BAR%u base=0x%llx size=0x%llx pref=%u",
                          static_cast<unsigned>(bus_id),
                          static_cast<unsigned>(device),
                          static_cast<unsigned>(function),
                          static_cast<unsigned>(current_bar),
                          static_cast<unsigned long long>(base),
                          static_cast<unsigned long long>(size),
                          prefetchable ? 1u : 0u);
                    return true;
                }
            }
        }
    }

    return false;
}

bool override_framebuffer_address_from_pci()
{
    if (!g_available)
    {
        return false;
    }

    uintptr_t detected_address = 0;
    if (!find_pci_vga_framebuffer(detected_address))
    {
        return false;
    }

    if (detected_address == 0 || detected_address == g_info.address)
    {
        return false;
    }

    g_info.address = detected_address;
    g_physical_address_low = static_cast<uint32_t>(detected_address & 0xFFFFFFFFULL);
    g_framebuffer_size = static_cast<uint32_t>(static_cast<uint64_t>(g_info.pitch) * static_cast<uint64_t>(g_info.height));

    debug("[FB] Overriding framebuffer address to 0x%llx via PCI",
          static_cast<unsigned long long>(detected_address));
    return true;
}

void bga_write(uint16_t index, uint16_t value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

uint16_t bga_read(uint16_t index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

bool adopt_framebuffer(const FrameBufferInfo &detected)
{
    if (detected.width == 0 || detected.height == 0 || detected.pitch == 0 || detected.bpp == 0 || detected.address == 0)
    {
        return false;
    }

    const uint32_t bytes_per_pixel = (detected.bpp + 7U) / 8U;
    if (bytes_per_pixel == 0)
    {
        return false;
    }

    const uint64_t size = static_cast<uint64_t>(detected.pitch) * static_cast<uint64_t>(detected.height);
    if (size == 0)
    {
        return false;
    }

    g_info = detected;
    g_bytes_per_pixel = bytes_per_pixel;
    g_physical_address_low = static_cast<uint32_t>(detected.address & 0xFFFFFFFFULL);
    g_framebuffer_size = static_cast<uint32_t>(size);
    g_available = true;
    return true;
}

bool bochs_available()
{
    const uint16_t id = bga_read(VBE_DISPI_INDEX_ID);
    return id >= VBE_DISPI_ID0 && id <= VBE_DISPI_ID5;
}

bool initialize_bochs(uint32_t width, uint32_t height, uint32_t bpp)
{
    if (!bochs_available())
    {
        debug("[FB] Bochs adapter not present");
        return false;
    }

    const uint32_t bytes_per_pixel = bpp / 8U;
    if (bytes_per_pixel == 0)
    {
        return false;
    }

    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, static_cast<uint16_t>(width));
    bga_write(VBE_DISPI_INDEX_YRES, static_cast<uint16_t>(height));
    bga_write(VBE_DISPI_INDEX_BPP, static_cast<uint16_t>(bpp));
    bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, static_cast<uint16_t>(width));
    bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, static_cast<uint16_t>(height));
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE, static_cast<uint16_t>(VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED));

    FrameBufferInfo detected{};
    detected.width = width;
    detected.height = height;
    detected.bpp = bpp;
    detected.pitch = width * bytes_per_pixel;
    detected.address = BOCHS_FRAMEBUFFER_PHYS;

    if (!adopt_framebuffer(detected))
    {
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        debug("[FB] Failed to adopt Bochs framebuffer configuration");
        return false;
    }

    debug("[FB] Bochs framebuffer configured %ux%u@%u", width, height, bpp);
    return true;
}

bool initialize_from_multiboot(const multiboot_info_t *info)
{
    if (!(info->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO))
    {
        debug("[FB] Multiboot framebuffer info flag not set");
        return false;
    }

    debug("[FB] width=%u height=%u pitch=%u bpp=%u addr=0x%llx",
          info->framebuffer_width,
          info->framebuffer_height,
          info->framebuffer_pitch,
          info->framebuffer_bpp,
          static_cast<unsigned long long>(info->framebuffer_addr));

    FrameBufferInfo detected{};
    detected.width = info->framebuffer_width;
    detected.height = info->framebuffer_height;
    detected.pitch = info->framebuffer_pitch;
    detected.bpp = info->framebuffer_bpp;
    detected.address = static_cast<uintptr_t>(info->framebuffer_addr);

    debug("[FB] vbe_mode_info=0x%x vbe_control_info=0x%x",
          info->vbe_mode_info,
          info->vbe_control_info);

    if ((detected.width == 0 || detected.height == 0 || detected.pitch == 0 || detected.bpp == 0 || detected.address == 0) &&
        (info->flags & MULTIBOOT_INFO_VBE_INFO) && info->vbe_mode_info != 0)
    {
        const VbeModeInfo *mode_info = reinterpret_cast<const VbeModeInfo *>(static_cast<uintptr_t>(info->vbe_mode_info));
        debug("[FB] VBE mode info ptr=%p", mode_info);
        if (mode_info != nullptr)
        {
            debug("[FB] VBE raw width=%u height=%u pitch=%u bpp=%u phys=0x%x",
                  mode_info->width,
                  mode_info->height,
                  mode_info->pitch,
                  mode_info->bpp,
                  mode_info->phys_base_ptr);
            detected.width = mode_info->width;
            detected.height = mode_info->height;
            detected.pitch = mode_info->pitch;
            detected.bpp = mode_info->bpp;
            detected.address = mode_info->phys_base_ptr;
        }
    }

    if (!adopt_framebuffer(detected))
    {
        debug("[FB] Multiboot framebuffer data invalid");
        return false;
    }

    debug("[FB] Initialized from multiboot info");
    return true;
}

inline uint8_t *framebuffer_ptr()
{
    return reinterpret_cast<uint8_t *>(g_info.address);
}

uint32_t pack_rgb24(uint8_t r, uint8_t g, uint8_t b)
{
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}
}

bool initialize(const multiboot_info_t *info)
{
    g_available = false;
    g_info = {};
    g_bytes_per_pixel = 0;
    g_physical_address_low = 0;
    g_framebuffer_size = 0;

    debug("[FB] Multiboot flags: 0x%x", info ? info->flags : 0U);

    if (info != nullptr && initialize_from_multiboot(info))
    {
        return true;
    }

    if (initialize_bochs(1024, 768, 32))
    {
        override_framebuffer_address_from_pci();
        return true;
    }

    return false;
}

bool is_available()
{
    return g_available;
}

const FrameBufferInfo &info()
{
    return g_info;
}

uint32_t framebuffer_physical_address()
{
    return g_physical_address_low;
}

uint32_t framebuffer_size()
{
    return g_framebuffer_size;
}

void update_address(uintptr_t new_address)
{
    g_info.address = new_address;
    g_physical_address_low = static_cast<uint32_t>(new_address & 0xFFFFFFFFULL);
}

uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_available)
    {
        return 0;
    }

    switch (g_bytes_per_pixel)
    {
    case 4:
    case 3:
        return pack_rgb24(r, g, b);
    case 2:
    {
        uint16_t value = static_cast<uint16_t>(((r & 0xF8) << 8) |
                                               ((g & 0xFC) << 3) |
                                               ((b & 0xF8) >> 3));
        return value;
    }
    default:
        return pack_rgb24(r, g, b);
    }
}

void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (!g_available)
    {
        return;
    }

    if (width == 0 || height == 0)
    {
        return;
    }

    if (x >= g_info.width || y >= g_info.height)
    {
        return;
    }

    if (x + width > g_info.width)
    {
        width = g_info.width - x;
    }
    if (y + height > g_info.height)
    {
        height = g_info.height - y;
    }

    uint8_t *base = framebuffer_ptr();
    const uint32_t row_stride = g_info.pitch;

    for (uint32_t row = 0; row < height; ++row)
    {
        uint8_t *row_ptr = base + (y + row) * row_stride + x * g_bytes_per_pixel;
        for (uint32_t col = 0; col < width; ++col)
        {
            uint8_t *pixel = row_ptr + col * g_bytes_per_pixel;
            switch (g_bytes_per_pixel)
            {
            case 4:
            {
                uint32_t value = color;
                memcpy(pixel, &value, sizeof(uint32_t));
                break;
            }
            case 3:
            {
                pixel[0] = static_cast<uint8_t>(color & 0xFF);
                pixel[1] = static_cast<uint8_t>((color >> 8) & 0xFF);
                pixel[2] = static_cast<uint8_t>((color >> 16) & 0xFF);
                break;
            }
            case 2:
            {
                uint16_t value = static_cast<uint16_t>(color & 0xFFFF);
                memcpy(pixel, &value, sizeof(uint16_t));
                break;
            }
            default:
                break;
            }
        }
    }
}

void draw_mono_bitmap(uint32_t x,
                      uint32_t y,
                      uint32_t width,
                      uint32_t height,
                      const uint8_t *bitmap,
                      uint32_t stride,
                      uint32_t fg_color,
                      uint32_t bg_color,
                      bool transparent_bg)
{
    if (!g_available || bitmap == nullptr)
    {
        return;
    }

    for (uint32_t row = 0; row < height; ++row)
    {
        const uint8_t *bitmap_row = bitmap + row * stride;
        for (uint32_t col = 0; col < width; ++col)
        {
            const uint32_t byte_index = col / 8;
            const uint8_t mask = static_cast<uint8_t>(0x80u >> (col & 7u));
            const bool bit_set = (bitmap_row[byte_index] & mask) != 0;
            if (!bit_set && transparent_bg)
            {
                continue;
            }
            uint32_t color = bit_set ? fg_color : bg_color;
            fill_rect(x + col, y + row, 1, 1, color);
        }
    }
}

uint32_t peek_pixel(uint32_t x, uint32_t y)
{
    if (!g_available)
    {
        return 0;
    }

    if (x >= g_info.width || y >= g_info.height)
    {
        return 0;
    }

    const uint8_t *base = framebuffer_ptr();
    const uint8_t *pixel = base + y * g_info.pitch + x * g_bytes_per_pixel;

    switch (g_bytes_per_pixel)
    {
    case 4:
    {
        uint32_t value;
        memcpy(&value, pixel, sizeof(uint32_t));
        return value;
    }
    case 3:
    {
        return static_cast<uint32_t>(pixel[0]) |
               (static_cast<uint32_t>(pixel[1]) << 8) |
               (static_cast<uint32_t>(pixel[2]) << 16);
    }
    case 2:
    {
        uint16_t value;
        memcpy(&value, pixel, sizeof(uint16_t));
        return value;
    }
    default:
        return 0;
    }
}

} // namespace framebuffer
