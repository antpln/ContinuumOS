#include <kernel/mouse.h>

#include <kernel/port_io.h>
#include <kernel/pic.h>
#include <kernel/isr.h>
#include <kernel/gui.h>
#include <kernel/vga.h>
#include <kernel/framebuffer.h>
#include <kernel/debug.h>
#include <kernel/scheduler.h>
#include <kernel/process.h>
#include <kernel/shell.h>
#include <kernel/hooks.h>

#include <stddef.h>

extern Terminal terminal;

namespace
{
constexpr uint16_t PS2_CMD_PORT = 0x64;
constexpr uint16_t PS2_DATA_PORT = 0x60;

constexpr uint8_t PS2_STATUS_OUTPUT_FULL = 0x01u;
constexpr uint8_t PS2_STATUS_INPUT_FULL = 0x02u;
constexpr uint8_t PS2_STATUS_MOUSE_DATA = 0x20u;

constexpr uint8_t PS2_ENABLE_AUX_DEVICE = 0xA8u;
constexpr uint8_t PS2_COMMAND_GET_STATUS = 0x20u;
constexpr uint8_t PS2_COMMAND_SET_STATUS = 0x60u;

constexpr uint8_t PS2_MOUSE_SET_DEFAULTS = 0xF6u;
constexpr uint8_t PS2_MOUSE_ENABLE_PACKET_STREAMING = 0xF4u;
constexpr uint8_t PS2_MOUSE_SET_SAMPLE_RATE = 0xF3u;
constexpr uint8_t PS2_MOUSE_GET_DEVICE_ID = 0xF2u;

constexpr uint8_t PS2_MOUSE_ACK = 0xFAu;

constexpr uint8_t IRQ_MOUSE = 12;
constexpr uint8_t ISR_MOUSE = static_cast<uint8_t>(32 + IRQ_MOUSE);

constexpr size_t MAX_PACKET_SIZE = 4;

volatile int32_t g_mouse_x = 0;
volatile int32_t g_mouse_y = 0;
volatile uint8_t g_mouse_buttons = 0;
volatile uint8_t g_mouse_available = 0;

MouseState g_state{};

uint8_t g_packet[MAX_PACKET_SIZE] = {};
uint8_t g_packet_index = 0;
uint8_t g_bytes_expected = 3;
bool g_has_scroll_wheel = false;

void mouse_wait(uint8_t type)
{
    // type 0 = wait for data, type 1 = wait for input clear
    int timeout = 100000;
    if (type == 0)
    {
        while (timeout-- > 0)
        {
            if (inb(PS2_CMD_PORT) & PS2_STATUS_OUTPUT_FULL)
            {
                return;
            }
        }
    }
    else
    {
        while (timeout-- > 0)
        {
            if ((inb(PS2_CMD_PORT) & PS2_STATUS_INPUT_FULL) == 0)
            {
                return;
            }
        }
    }
}

void mouse_flush_output()
{
    for (int i = 0; i < 256; ++i)
    {
        if ((inb(PS2_CMD_PORT) & PS2_STATUS_OUTPUT_FULL) == 0)
        {
            break;
        }
        (void)inb(PS2_DATA_PORT);
    }
}

void mouse_write(uint8_t data)
{
    mouse_wait(1);
    outb(PS2_CMD_PORT, 0xD4);
    mouse_wait(1);
    outb(PS2_DATA_PORT, data);
}

uint8_t mouse_read()
{
    mouse_wait(0);
    return inb(PS2_DATA_PORT);
}

bool mouse_send_command(uint8_t command)
{
    mouse_write(command);
    uint8_t response = mouse_read();
    return response == PS2_MOUSE_ACK;
}

bool mouse_send_command(uint8_t command, uint8_t value)
{
    if (!mouse_send_command(command))
    {
        return false;
    }
    mouse_write(value);
    return mouse_read() == PS2_MOUSE_ACK;
}

void clamp_position()
{
    if (!framebuffer::is_available())
    {
        if (g_mouse_x < 0)
        {
            g_mouse_x = 0;
        }
        if (g_mouse_y < 0)
        {
            g_mouse_y = 0;
        }
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0)
    {
        g_mouse_x = 0;
        g_mouse_y = 0;
        return;
    }

    if (g_mouse_x < 0)
    {
        g_mouse_x = 0;
    }
    else if (g_mouse_x >= static_cast<int32_t>(fb.width))
    {
        g_mouse_x = static_cast<int32_t>(fb.width) - 1;
    }

    if (g_mouse_y < 0)
    {
        g_mouse_y = 0;
    }
    else if (g_mouse_y >= static_cast<int32_t>(fb.height))
    {
        g_mouse_y = static_cast<int32_t>(fb.height) - 1;
    }
}

void dispatch_event(MouseEvent &event)
{
    Process *target = scheduler_get_foreground();
    if (target == nullptr)
    {
        target = shell_get_process();
    }
    if (target == nullptr)
    {
        target = scheduler_current_process();
    }

    if (target == nullptr)
    {
        return;
    }

    event.target_pid = target->pid;

    IOEvent io_event{};
    io_event.type = EVENT_MOUSE;
    io_event.data.mouse = event;
    push_io_event(target, io_event);
    scheduler_resume_processes_for_event(HookType::SIGNAL, static_cast<uint64_t>(target->pid));
}

void handle_packet()
{
    const uint8_t status = g_packet[0];
    const bool x_overflow = (status & 0x40u) != 0;
    const bool y_overflow = (status & 0x80u) != 0;
    if (x_overflow || y_overflow)
    {
        g_packet_index = 0;
        return;
    }

    const int16_t dx = static_cast<int8_t>(g_packet[1]);
    const int16_t dy_raw = static_cast<int8_t>(g_packet[2]);
    const int16_t dy = static_cast<int16_t>(-dy_raw);

    const int32_t previous_x = g_mouse_x;
    const int32_t previous_y = g_mouse_y;

    g_mouse_x += dx;
    g_mouse_y += dy;
    clamp_position();

    const uint8_t buttons = static_cast<uint8_t>(status & 0x07u);
    const uint8_t changed = static_cast<uint8_t>(buttons ^ g_mouse_buttons);

    g_mouse_buttons = buttons;

    int8_t scroll_x = 0;
    int8_t scroll_y = 0;
    if (g_has_scroll_wheel && g_bytes_expected == 4)
    {
        scroll_y = static_cast<int8_t>(g_packet[3]);
    }

    g_state.x = g_mouse_x;
    g_state.y = g_mouse_y;
    g_state.buttons = g_mouse_buttons;
    g_state.available = g_mouse_available;

    MouseEvent event{};
    event.x = g_mouse_x;
    event.y = g_mouse_y;
    event.dx = static_cast<int16_t>(g_mouse_x - previous_x);
    event.dy = static_cast<int16_t>(g_mouse_y - previous_y);
    event.scroll_x = scroll_x;
    event.scroll_y = scroll_y;
    event.buttons = buttons;
    event.changed = changed;
    event.target_pid = -1;

    gui::handle_mouse_event(event, terminal);
    dispatch_event(event);
}

void mouse_callback(registers_t *regs)
{
    (void)regs;

    uint8_t status = inb(PS2_CMD_PORT);
    while (status & PS2_STATUS_OUTPUT_FULL)
    {
        if ((status & PS2_STATUS_MOUSE_DATA) == 0)
        {
            break;
        }

        uint8_t data = inb(PS2_DATA_PORT);

        if (g_packet_index == 0 && (data & 0x08u) == 0)
        {
            status = inb(PS2_CMD_PORT);
            continue;
        }

        g_packet[g_packet_index++] = data;
        if (g_packet_index >= g_bytes_expected)
        {
            handle_packet();
            g_packet_index = 0;
        }

        status = inb(PS2_CMD_PORT);
    }

    pic_send_eoi(IRQ_MOUSE);
}

bool try_enable_scroll_wheel()
{
    bool success = mouse_send_command(PS2_MOUSE_SET_SAMPLE_RATE, 200) &&
                   mouse_send_command(PS2_MOUSE_SET_SAMPLE_RATE, 100) &&
                   mouse_send_command(PS2_MOUSE_SET_SAMPLE_RATE, 80);

    if (!success)
    {
        return false;
    }

    mouse_write(PS2_MOUSE_GET_DEVICE_ID);
    if (mouse_read() != PS2_MOUSE_ACK)
    {
        return false;
    }
    const uint8_t id = mouse_read();
    return id == 0x03u || id == 0x04u;
}
} // namespace

void mouse_initialize()
{
    debug("[MOUSE] Initializing PS/2 mouse");

    g_packet_index = 0;
    g_bytes_expected = 3;
    g_has_scroll_wheel = false;
    g_mouse_buttons = 0;

    mouse_flush_output();

    mouse_wait(1);
    outb(PS2_CMD_PORT, PS2_ENABLE_AUX_DEVICE);

    mouse_wait(1);
    outb(PS2_CMD_PORT, PS2_COMMAND_GET_STATUS);
    mouse_wait(0);
    uint8_t status = inb(PS2_DATA_PORT);
    status |= 0x02u; // Enable IRQ12
    status |= 0x20u; // Enable mouse clock

    mouse_wait(1);
    outb(PS2_CMD_PORT, PS2_COMMAND_SET_STATUS);
    mouse_wait(1);
    outb(PS2_DATA_PORT, status);

    if (!mouse_send_command(PS2_MOUSE_SET_DEFAULTS))
    {
        error("[MOUSE] Failed to set defaults");
    }

    g_has_scroll_wheel = try_enable_scroll_wheel();
    g_bytes_expected = g_has_scroll_wheel ? 4 : 3;

    if (!mouse_send_command(PS2_MOUSE_SET_DEFAULTS))
    {
        error("[MOUSE] Failed to reset defaults after ID probe");
    }

    if (!mouse_send_command(PS2_MOUSE_ENABLE_PACKET_STREAMING))
    {
        error("[MOUSE] Failed to enable streaming");
    }

    register_interrupt_handler(ISR_MOUSE, mouse_callback);
    pic_unmask_irq(IRQ_MOUSE);

    g_mouse_available = framebuffer::is_available() ? 1 : 0;

    if (framebuffer::is_available())
    {
        const auto &fb = framebuffer::info();
        g_mouse_x = static_cast<int32_t>(fb.width / 2);
        g_mouse_y = static_cast<int32_t>(fb.height / 2);
    }
    else
    {
        g_mouse_x = 0;
        g_mouse_y = 0;
    }

    clamp_position();

    g_state.x = g_mouse_x;
    g_state.y = g_mouse_y;
    g_state.buttons = g_mouse_buttons;
    g_state.available = g_mouse_available;

    if (g_mouse_available)
    {
        gui::initialize_mouse_cursor(g_mouse_x, g_mouse_y, g_mouse_buttons);
    }

    debug("[MOUSE] Initialized (wheel=%u, packet_bytes=%u, pos=%d,%d)",
          g_has_scroll_wheel ? 1u : 0u,
          static_cast<unsigned>(g_bytes_expected),
          static_cast<int>(g_mouse_x),
          static_cast<int>(g_mouse_y));
}

MouseState mouse_get_state()
{
    return g_state;
}
