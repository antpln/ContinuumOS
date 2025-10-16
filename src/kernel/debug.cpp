#include <kernel/debug.h>
#include <kernel/framebuffer.h>
#include <kernel/font8x16.h>
#include <kernel/gui.h>
#include <kernel/serial.h>
#include <kernel/vga.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


extern Terminal terminal;

namespace
{
int simple_vsnprintf(char *buffer, size_t size, const char *fmt, va_list args)
{
    size_t pos = 0;
    size_t total = 0;

    auto append_char = [&](char c) {
        if (pos + 1 < size)
        {
            buffer[pos] = c;
        }
        ++pos;
        ++total;
    };

    auto append_string = [&](const char *str) {
        if (str == nullptr)
        {
            str = "(null)";
        }
        while (*str)
        {
            append_char(*str++);
        }
    };

    auto append_unsigned = [&](uint64_t value, unsigned base, bool uppercase) {
        char digits[32];
        const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        size_t idx = 0;
        do
        {
            digits[idx++] = alphabet[value % base];
            value /= base;
        } while (value != 0 && idx < sizeof(digits));

        while (idx > 0)
        {
            append_char(digits[--idx]);
        }
    };

    const char *cursor = fmt;
    while (cursor != nullptr && *cursor != '\0')
    {
        char ch = *cursor++;
        if (ch != '%')
        {
            append_char(ch);
            continue;
        }

        if (*cursor == '%')
        {
            append_char('%');
            ++cursor;
            continue;
        }

        char spec = *cursor++;
        switch (spec)
        {
        case 'd':
        case 'i':
        {
            int value = va_arg(args, int);
            if (value < 0)
            {
                append_char('-');
                append_unsigned(static_cast<uint64_t>(-static_cast<int64_t>(value)), 10, false);
            }
            else
            {
                append_unsigned(static_cast<uint64_t>(value), 10, false);
            }
            break;
        }
        case 'u':
        {
            unsigned value = va_arg(args, unsigned);
            append_unsigned(value, 10, false);
            break;
        }
        case 'x':
        case 'X':
        {
            unsigned value = va_arg(args, unsigned);
            append_unsigned(value, 16, spec == 'X');
            break;
        }
        case 'p':
        {
            uintptr_t value = reinterpret_cast<uintptr_t>(va_arg(args, void *));
            append_string("0x");
            append_unsigned(static_cast<uint64_t>(value), 16, false);
            break;
        }
        case 'c':
        {
            int value = va_arg(args, int);
            append_char(static_cast<char>(value));
            break;
        }
        case 's':
        {
            const char *str = va_arg(args, const char *);
            append_string(str);
            break;
        }
        default:
            append_char('%');
            append_char(spec);
            break;
        }
    }

    if (size > 0)
    {
        if (pos < size)
        {
            buffer[pos] = '\0';
        }
        else
        {
            buffer[size - 1] = '\0';
        }
    }

    return static_cast<int>(total);
}

int simple_snprintf(char *buffer, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = simple_vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return result;
}

constexpr uint32_t PANEL_PADDING = 32;
constexpr uint32_t PANEL_BORDER = 2;
constexpr uint32_t TITLE_GAP = 12;
constexpr uint32_t SECTION_GAP = 14;
constexpr uint32_t LINE_SPACING = 6;
constexpr size_t WRAP_BUFFER_SIZE = 512;

uint32_t clamp_width(uint32_t x, uint32_t requested, uint32_t fb_width)
{
    if (fb_width == 0 || x >= fb_width)
    {
        return 0;
    }

    if (requested == 0 || x + requested > fb_width)
    {
        return fb_width - x;
    }

    return requested;
}

void draw_text_line(uint32_t x, uint32_t y, uint32_t max_width, uint32_t color, const char *text)
{
    if (!framebuffer::is_available() || text == nullptr)
    {
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0 || x >= fb.width || y >= fb.height)
    {
        return;
    }

    const uint32_t width = clamp_width(x, max_width, fb.width);
    if (width < gui::FONT_WIDTH || y + gui::FONT_HEIGHT > fb.height)
    {
        return;
    }

    const uint32_t limit_x = x + width;

    uint32_t pen_x = x;
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        const char ch = text[i];
        if (ch == '\n')
        {
            break;
        }

        if (pen_x + gui::FONT_WIDTH > limit_x)
        {
            break;
        }

        const uint8_t *glyph = gui::glyph_for(ch);
        framebuffer::draw_mono_bitmap(pen_x, y, gui::FONT_WIDTH, gui::FONT_HEIGHT, glyph, 1, color, 0, true);
        pen_x += gui::FONT_WIDTH;
    }
}

void draw_wrapped_text(uint32_t x, uint32_t &y, uint32_t max_width, uint32_t color, const char *text, uint32_t line_spacing = LINE_SPACING)
{
    if (!framebuffer::is_available() || text == nullptr)
    {
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0 || x >= fb.width || y >= fb.height)
    {
        return;
    }

    const uint32_t width = clamp_width(x, max_width, fb.width);
    if (width < gui::FONT_WIDTH)
    {
        return;
    }

    const uint32_t max_chars = width / gui::FONT_WIDTH;
    if (max_chars == 0)
    {
        return;
    }

    const uint32_t effective_max_chars = max_chars < (WRAP_BUFFER_SIZE - 1) ? max_chars : (WRAP_BUFFER_SIZE - 1);
    if (effective_max_chars == 0)
    {
        return;
    }

    char line_buffer[WRAP_BUFFER_SIZE];
    const uint32_t line_height = gui::FONT_HEIGHT + line_spacing;

    const char *cursor = text;
    while (*cursor != '\0' && y < fb.height)
    {
        if (*cursor == '\n')
        {
            y += line_height;
            ++cursor;
            continue;
        }

        size_t count = 0;
        while (count < effective_max_chars && cursor[count] != '\0' && cursor[count] != '\n')
        {
            ++count;
        }

        if (count == 0)
        {
            ++cursor;
            continue;
        }

        memcpy(line_buffer, cursor, count);
        line_buffer[count] = '\0';

        if (y + gui::FONT_HEIGHT > fb.height)
        {
            break;
        }

        draw_text_line(x, y, width, color, line_buffer);
        y += line_height;

        cursor += count;
        if (*cursor == '\n')
        {
            ++cursor;
        }
    }
}

void render_gui_panic_screen(const char *message,
                             const char *file,
                             int line,
                             const char *function,
                             const char *details)
{
    if (!framebuffer::is_available())
    {
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0)
    {
        return;
    }

    const uint8_t top_r = 110;
    const uint8_t top_g = 28;
    const uint8_t top_b = 36;
    const uint8_t bottom_r = 46;
    const uint8_t bottom_g = 6;
    const uint8_t bottom_b = 12;

    for (uint32_t row = 0; row < fb.height; ++row)
    {
        const uint32_t mix = fb.height > 1 ? (row * 255U) / (fb.height - 1U) : 0U;
        const uint32_t inv = 255U - mix;
        const uint8_t r = static_cast<uint8_t>((top_r * inv + bottom_r * mix) / 255U);
        const uint8_t g = static_cast<uint8_t>((top_g * inv + bottom_g * mix) / 255U);
        const uint8_t b = static_cast<uint8_t>((top_b * inv + bottom_b * mix) / 255U);
        framebuffer::fill_rect(0, row, fb.width, 1, framebuffer::pack_color(r, g, b));
    }

    uint32_t margin_x = fb.width / 8U;
    uint32_t margin_y = fb.height / 6U;
    if (margin_x < PANEL_PADDING)
    {
        margin_x = PANEL_PADDING;
    }
    if (margin_y < PANEL_PADDING)
    {
        margin_y = PANEL_PADDING;
    }

    uint32_t panel_x = 0;
    uint32_t panel_y = 0;
    uint32_t panel_width = fb.width;
    uint32_t panel_height = fb.height;

    if (fb.width > 2U * margin_x)
    {
        panel_x = margin_x;
        panel_width = fb.width - 2U * margin_x;
    }
    if (fb.height > 2U * margin_y)
    {
        panel_y = margin_y;
        panel_height = fb.height - 2U * margin_y;
    }

    if (panel_width < 2U * PANEL_PADDING)
    {
        panel_x = 0;
        panel_width = fb.width;
    }
    if (panel_height < 2U * PANEL_PADDING)
    {
        panel_y = 0;
        panel_height = fb.height;
    }

    const uint32_t panel_color = framebuffer::pack_color(18, 20, 30);
    framebuffer::fill_rect(panel_x, panel_y, panel_width, panel_height, panel_color);

    const uint32_t border_color = framebuffer::pack_color(210, 70, 80);
    if (panel_width > 2U * PANEL_BORDER && panel_height > 2U * PANEL_BORDER)
    {
        framebuffer::fill_rect(panel_x, panel_y, panel_width, PANEL_BORDER, border_color);
        framebuffer::fill_rect(panel_x, panel_y + panel_height - PANEL_BORDER, panel_width, PANEL_BORDER, border_color);
        framebuffer::fill_rect(panel_x, panel_y, PANEL_BORDER, panel_height, border_color);
        framebuffer::fill_rect(panel_x + panel_width - PANEL_BORDER, panel_y, PANEL_BORDER, panel_height, border_color);
    }

    const uint32_t accent_color = framebuffer::pack_color(255, 128, 140);
    const uint32_t heading_color = framebuffer::pack_color(255, 204, 210);
    const uint32_t body_color = framebuffer::pack_color(236, 238, 246);
    const uint32_t helper_color = framebuffer::pack_color(210, 180, 186);

    const uint32_t content_x = panel_x + PANEL_PADDING;
    uint32_t content_width = panel_width > 2U * PANEL_PADDING ? panel_width - 2U * PANEL_PADDING : panel_width;
    uint32_t text_y = panel_y + PANEL_PADDING;

    if (content_x >= fb.width)
    {
        return;
    }
    if (content_width > fb.width - content_x)
    {
        content_width = fb.width - content_x;
    }

    draw_text_line(content_x, text_y, content_width, accent_color, ":(");
    text_y += gui::FONT_HEIGHT + TITLE_GAP;

    draw_text_line(content_x, text_y, content_width, heading_color, "Kernel Panic");
    text_y += gui::FONT_HEIGHT + SECTION_GAP;

    draw_wrapped_text(content_x,
                      text_y,
                      content_width,
                      body_color,
                      "A critical error occurred and the kernel must stop.",
                      LINE_SPACING);
    text_y += SECTION_GAP;

    char buffer[WRAP_BUFFER_SIZE];

    if (message != nullptr)
    {
        simple_snprintf(buffer, sizeof(buffer), "Message: %s", message);
        draw_wrapped_text(content_x, text_y, content_width, body_color, buffer, LINE_SPACING);
        text_y += SECTION_GAP;
    }

    if (details != nullptr && details[0] != '\0')
    {
        simple_snprintf(buffer, sizeof(buffer), "Details: %s", details);
        draw_wrapped_text(content_x, text_y, content_width, body_color, buffer, LINE_SPACING);
        text_y += SECTION_GAP;
    }

    if (file != nullptr)
    {
        simple_snprintf(buffer, sizeof(buffer), "Location: %s:%d", file, line);
    }
    else
    {
        simple_snprintf(buffer, sizeof(buffer), "Location: line %d", line);
    }
    draw_wrapped_text(content_x, text_y, content_width, body_color, buffer, LINE_SPACING);
    text_y += SECTION_GAP;

    if (function != nullptr)
    {
        simple_snprintf(buffer, sizeof(buffer), "Function: %s", function);
        draw_wrapped_text(content_x, text_y, content_width, body_color, buffer, LINE_SPACING);
        text_y += SECTION_GAP;
    }

    draw_wrapped_text(content_x,
                      text_y,
                      content_width,
                      helper_color,
                      "System halted. Check the serial console for additional information.",
                      LINE_SPACING);
}
} // namespace

static bool format_has_placeholders(const char* fmt) {
    if (!fmt) {
        return false;
    }

    while (*fmt) {
        if (*fmt == '%') {
            ++fmt;
            if (*fmt == '%') {
                ++fmt;
                continue;
            }
            return true;
        }
        ++fmt;
    }
    return false;
}

void panic(const char* msg, const char* file, int line, const char* func, ...) {
    const char* display_message = msg != nullptr ? msg : "<no message>";
    const char* display_file = file != nullptr ? file : "<unknown>";
    const char* display_func = func != nullptr ? func : "<unknown>";

    va_list args;
    va_start(args, func);

    char details_buffer[WRAP_BUFFER_SIZE];
    details_buffer[0] = '\0';

    const bool has_details = format_has_placeholders(display_message);
    if (has_details) {
        va_list details_args;
        va_copy(details_args, args);
        simple_vsnprintf(details_buffer, sizeof(details_buffer), display_message, details_args);
        va_end(details_args);
    }

    terminal.setfull_color(VGA_COLOR_BLACK, VGA_COLOR_RED);
    terminal.clear();
    // Sad face ASCII art
    printf("\n\n        :(\n");
    printf("\n================ KERNEL PANIC ================\n");
    printf("A critical error occurred and the kernel must stop.\n\n");
    printf("Message: %s\n", display_message);
    printf("Location: %s:%d\n", display_file, line);
    printf("Function: %s\n", display_func);
    if (has_details) {
        printf("Details: %s\n", details_buffer);
    }
    printf("\n==============================================\n");

    serial_write("\n\n        :(\n");
    serial_write("\n================ KERNEL PANIC ================\n");
    serial_write("A critical error occurred and the kernel must stop.\n\n");
    serial_printf("Message: %s\n", display_message);
    serial_printf("Location: %s:%d\n", display_file, line);
    serial_printf("Function: %s\n", display_func);
    if (has_details) {
        serial_printf("Details: %s\n", details_buffer);
    }
    serial_write("\n==============================================\n");

    render_gui_panic_screen(display_message, display_file, line, display_func, has_details ? details_buffer : nullptr);

    va_end(args);

    while (1) {
        __asm__("cli; hlt");
    }
}
void debug(const char* fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[DEBUG] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[DEBUG] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
void success(const char* fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[SUCCESS] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[SUCCESS] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[ERROR] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[ERROR] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
}
void test(const char* fmt, ...) {
#ifdef TEST
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[TEST] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[TEST] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
