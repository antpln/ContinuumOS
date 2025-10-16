#include <kernel/gui.h>

#include <kernel/framebuffer.h>
#include <kernel/terminal_windows.h>
#include <kernel/debug.h>

namespace gui
{
namespace
{
uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return framebuffer::pack_color(r, g, b);
}

struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr Color TOP_COLOR{24, 28, 38};
constexpr Color BOTTOM_COLOR{10, 14, 22};

bool g_background_override_active = false;
uint32_t g_background_override_color = 0;

uint32_t gradient_color(uint32_t y)
{
    if (!framebuffer::is_available())
    {
        return 0;
    }

    const auto &fb = framebuffer::info();
    if (fb.height == 0)
    {
        return pack_rgb(TOP_COLOR.r, TOP_COLOR.g, TOP_COLOR.b);
    }

    uint32_t clamped_y = y;
    if (clamped_y >= fb.height)
    {
        clamped_y = fb.height - 1;
    }

    const uint32_t mix = fb.height > 1 ? (clamped_y * 255U) / (fb.height - 1U) : 0U;
    const uint32_t inv = 255U - mix;

    const uint8_t r = static_cast<uint8_t>((TOP_COLOR.r * inv + BOTTOM_COLOR.r * mix) / 255U);
    const uint8_t g = static_cast<uint8_t>((TOP_COLOR.g * inv + BOTTOM_COLOR.g * mix) / 255U);
    const uint8_t b = static_cast<uint8_t>((TOP_COLOR.b * inv + BOTTOM_COLOR.b * mix) / 255U);

    return pack_rgb(r, g, b);
}

void draw_background_gradient()
{
    if (!framebuffer::is_available())
    {
        return;
    }

    const auto &fb = framebuffer::info();
    fill_background_rect(0, 0, fb.width, fb.height);
}
} // namespace

void draw_boot_screen()
{
    debug("[GUI] draw_boot_screen");
    clear_background_fill_override();
    draw_background_gradient();
}

void draw_workspace(Terminal &terminal)
{
    debug("[GUI] draw_workspace");
    if (!framebuffer::is_available())
    {
        return;
    }

    draw_background_gradient();
    terminal_windows::draw_windows(terminal);
}

void process_command(const GuiCommand &command, Terminal &terminal, Process *requester)
{
    switch (command.type)
    {
    case GUI_COMMAND_REDRAW:
        draw_workspace(terminal);
        break;
    case GUI_COMMAND_SET_TERMINAL_ORIGIN:
        if (requester != nullptr)
        {
            terminal_windows::set_active_window_origin(terminal, requester, command.arg0, command.arg1);
            draw_workspace(terminal);
        }
        break;
    case GUI_COMMAND_REQUEST_NEW_WINDOW:
        if (requester != nullptr)
        {
            terminal_windows::request_new_window(terminal, requester);
            draw_workspace(terminal);
        }
        break;
    default:
        break;
    }
}

uint32_t background_color_for_row(uint32_t y)
{
    return gradient_color(y);
}

void fill_background_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!framebuffer::is_available() || width == 0 || height == 0)
    {
        return;
    }

    const auto &fb = framebuffer::info();
    if (x >= fb.width || y >= fb.height)
    {
        return;
    }

    if (g_background_override_active)
    {
        framebuffer::fill_rect(x, y, width, height, g_background_override_color);
        return;
    }

    for (uint32_t row = 0; row < height; ++row)
    {
        const uint32_t current_y = y + row;
        if (current_y >= fb.height)
        {
            break;
        }

        const uint32_t color = gradient_color(current_y);
        framebuffer::fill_rect(x, current_y, width, 1, color);
    }
}

void set_background_fill_override(uint32_t color)
{
    g_background_override_active = true;
    g_background_override_color = color;
}

void clear_background_fill_override()
{
    g_background_override_active = false;
}

} // namespace gui
