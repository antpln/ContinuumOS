#include <kernel/gui.h>

#include <kernel/framebuffer.h>
#include <kernel/terminal_windows.h>
#include <kernel/debug.h>
#include <kernel/mouse.h>

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

constexpr uint32_t CURSOR_WIDTH = 16;
constexpr uint32_t CURSOR_HEIGHT = 16;
constexpr uint32_t CURSOR_STRIDE = (CURSOR_WIDTH + 7) / 8;

constexpr uint8_t CURSOR_OUTLINE[CURSOR_HEIGHT * CURSOR_STRIDE] = {
    0x01, 0x00, // Row 0: .......B........
    0x01, 0x00, // Row 1: .......B........
    0x01, 0x00, // Row 2: .......B........
    0x01, 0x00, // Row 3: .......B........
    0x01, 0x00, // Row 4: .......B........
    0x01, 0x00, // Row 5: .......B........
    0x01, 0x00, // Row 6: .......B........
    0xFF, 0xFF, // Row 7: BBBBBBBBBBBBBBBB
    0x01, 0x00, // Row 8: .......B........
    0x01, 0x00, // Row 9: .......B........
    0x01, 0x00, // Row 10: .......B........
    0x01, 0x00, // Row 11: .......B........
    0x01, 0x00, // Row 12: .......B........
    0x01, 0x00, // Row 13: .......B........
    0x01, 0x00, // Row 14: .......B........
    0x01, 0x00, // Row 15: .......B........
};

constexpr uint8_t CURSOR_FILL[CURSOR_HEIGHT * CURSOR_STRIDE] = {
    0x01, 0x00, // Row 0: .......B........
    0x01, 0x00, // Row 1: .......B........
    0x01, 0x00, // Row 2: .......B........
    0x01, 0x00, // Row 3: .......B........
    0x01, 0x00, // Row 4: .......B........
    0x01, 0x00, // Row 5: .......B........
    0x01, 0x00, // Row 6: .......B........
    0xFF, 0xFF, // Row 7: BBBBBBBBBBBBBBBB
    0x01, 0x00, // Row 8: .......B........
    0x01, 0x00, // Row 9: .......B........
    0x01, 0x00, // Row 10: .......B........
    0x01, 0x00, // Row 11: .......B........
    0x01, 0x00, // Row 12: .......B........
    0x01, 0x00, // Row 13: .......B........
    0x01, 0x00, // Row 14: .......B........
    0x01, 0x00, // Row 15: .......B........
};
struct CursorState
{
    int32_t x = 0;
    int32_t y = 0;
    uint8_t buttons = 0;
    bool visible = false;
    bool drawn = false;
    uint32_t saved_height = 0;
    uint32_t row_width[CURSOR_HEIGHT] = {0};
    uint32_t background[CURSOR_HEIGHT][CURSOR_WIDTH] = {{0}};
};

CursorState g_cursor{};

void clamp_cursor()
{
    if (!framebuffer::is_available())
    {
        if (g_cursor.x < 0)
        {
            g_cursor.x = 0;
        }
        if (g_cursor.y < 0)
        {
            g_cursor.y = 0;
        }
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0)
    {
        g_cursor.x = 0;
        g_cursor.y = 0;
        return;
    }

    if (g_cursor.x < 0)
    {
        g_cursor.x = 0;
    }
    else if (g_cursor.x >= static_cast<int32_t>(fb.width))
    {
        g_cursor.x = static_cast<int32_t>(fb.width) - 1;
    }

    if (g_cursor.y < 0)
    {
        g_cursor.y = 0;
    }
    else if (g_cursor.y >= static_cast<int32_t>(fb.height))
    {
        g_cursor.y = static_cast<int32_t>(fb.height) - 1;
    }
}

void capture_cursor_background()
{
    if (!framebuffer::is_available())
    {
        return;
    }

    const auto &fb = framebuffer::info();
    g_cursor.saved_height = 0;

    for (uint32_t row = 0; row < CURSOR_HEIGHT; ++row)
    {
        const uint32_t py = static_cast<uint32_t>(g_cursor.y + static_cast<int32_t>(row));
        if (py >= fb.height)
        {
            g_cursor.row_width[row] = 0;
            continue;
        }

        uint32_t width = 0;
        for (uint32_t col = 0; col < CURSOR_WIDTH; ++col)
        {
            const uint32_t px = static_cast<uint32_t>(g_cursor.x + static_cast<int32_t>(col));
            if (px >= fb.width)
            {
                break;
            }
            g_cursor.background[row][col] = framebuffer::peek_pixel(px, py);
            width = col + 1;
        }
        g_cursor.row_width[row] = width;
        if (width > 0)
        {
            g_cursor.saved_height = row + 1;
        }
    }
}

void restore_cursor_background()
{
    if (!framebuffer::is_available() || !g_cursor.drawn)
    {
        return;
    }

    const auto &fb = framebuffer::info();
    for (uint32_t row = 0; row < g_cursor.saved_height; ++row)
    {
        const uint32_t width = g_cursor.row_width[row];
        if (width == 0)
        {
            continue;
        }
        const uint32_t py = static_cast<uint32_t>(g_cursor.y + static_cast<int32_t>(row));
        if (py >= fb.height)
        {
            continue;
        }
        for (uint32_t col = 0; col < width; ++col)
        {
            const uint32_t px = static_cast<uint32_t>(g_cursor.x + static_cast<int32_t>(col));
            if (px >= fb.width)
            {
                break;
            }
            framebuffer::fill_rect(px, py, 1, 1, g_cursor.background[row][col]);
        }
    }
    g_cursor.saved_height = 0;
    g_cursor.drawn = false;
}

void render_mouse_cursor()
{
    if (!framebuffer::is_available() || !g_cursor.visible)
    {
        return;
    }

    const auto &fb = framebuffer::info();
    if (fb.width == 0 || fb.height == 0)
    {
        return;
    }

    clamp_cursor();
    capture_cursor_background();

    const uint32_t outline_color = framebuffer::pack_color(16, 20, 28);
    const bool button_active = (g_cursor.buttons & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT | MOUSE_BUTTON_MIDDLE)) != 0;
    const uint32_t fill_color = button_active ? framebuffer::pack_color(198, 220, 255)
                                              : framebuffer::pack_color(248, 248, 255);

    framebuffer::draw_mono_bitmap(static_cast<uint32_t>(g_cursor.x),
                                  static_cast<uint32_t>(g_cursor.y),
                                  CURSOR_WIDTH,
                                  CURSOR_HEIGHT,
                                  CURSOR_OUTLINE,
                                  CURSOR_STRIDE,
                                  outline_color,
                                  0,
                                  true);
    framebuffer::draw_mono_bitmap(static_cast<uint32_t>(g_cursor.x),
                                  static_cast<uint32_t>(g_cursor.y),
                                  CURSOR_WIDTH,
                                  CURSOR_HEIGHT,
                                  CURSOR_FILL,
                                  CURSOR_STRIDE,
                                  fill_color,
                                  outline_color,
                                  true);

    g_cursor.drawn = true;
}

void suspend_mouse_cursor()
{
    if (!g_cursor.drawn)
    {
        return;
    }
    restore_cursor_background();
}

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
    suspend_mouse_cursor();
    draw_background_gradient();
    render_mouse_cursor();
}

void draw_workspace(Terminal &terminal)
{
    debug("[GUI] draw_workspace");
    if (!framebuffer::is_available())
    {
        return;
    }

    begin_window_redraw();
    draw_background_gradient();
    terminal_windows::draw_windows(terminal);
    end_window_redraw();
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

void initialize_mouse_cursor(int32_t x, int32_t y, uint8_t buttons)
{
    if (!framebuffer::is_available())
    {
        return;
    }

    g_cursor.visible = true;
    suspend_mouse_cursor();
    g_cursor.x = x;
    g_cursor.y = y;
    g_cursor.buttons = buttons;
    clamp_cursor();
    render_mouse_cursor();
}

void handle_mouse_event(const MouseEvent &event, Terminal &terminal)
{
    if (!framebuffer::is_available())
    {
        return;
    }

    g_cursor.visible = true;

    const bool position_changed = (event.x != g_cursor.x) || (event.y != g_cursor.y);
    const bool buttons_changed = (event.buttons != g_cursor.buttons);
    const bool cursor_changed = position_changed || buttons_changed;

    if (cursor_changed)
    {
        suspend_mouse_cursor();
        g_cursor.x = event.x;
        g_cursor.y = event.y;
        g_cursor.buttons = event.buttons;
        clamp_cursor();
    }

    const bool consumed = terminal_windows::handle_mouse_event(terminal, event);
    (void)consumed;

    if (g_cursor.visible && !g_cursor.drawn)
    {
        render_mouse_cursor();
    }
}

void begin_window_redraw()
{
    suspend_mouse_cursor();
}

void end_window_redraw()
{
    if (g_cursor.visible && !g_cursor.drawn)
    {
        render_mouse_cursor();
    }
}

} // namespace gui
