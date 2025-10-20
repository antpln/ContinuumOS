#include <kernel/graphics.h>

#include <kernel/framebuffer.h>
#include <kernel/terminal_windows.h>
#include <kernel/scheduler.h>
#include <kernel/process.h>

extern Terminal terminal;

namespace graphics
{
namespace
{
Process *current_process()
{
    return scheduler_current_process();
}

bool ensure_window_for(Process *proc)
{
    if (proc == nullptr || !framebuffer::is_available())
    {
        return false;
    }

    size_t dummy_row = 0;
    size_t dummy_col = 0;
    if (!terminal_windows::window_get_cursor(proc, dummy_row, dummy_col))
    {
        terminal_windows::request_new_window(terminal, proc);
    }

    return terminal_windows::window_get_cursor(proc, dummy_row, dummy_col);
}
} // namespace

Color make_color(vga_color foreground, vga_color background)
{
    return terminal.make_color(foreground, background);
}

size_t columns()
{
    return Terminal::VGA_WIDTH;
}

size_t rows()
{
    return Terminal::VGA_HEIGHT;
}

void ensure_window()
{
    (void)ensure_window_for(current_process());
}

void clear(Color color, char fill_char)
{
    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    const size_t max_columns = columns();
    const size_t max_rows = rows();

    for (size_t row = 0; row < max_rows; ++row)
    {
        for (size_t col = 0; col < max_columns; ++col)
        {
            terminal_windows::window_put_char(proc, col, row, fill_char, color);
        }
    }

    terminal_windows::window_set_cursor(proc, 0, 0, false);
}

void put_char(size_t column, size_t row, char ch, Color color)
{
    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    if (column >= columns() || row >= rows())
    {
        return;
    }

    terminal_windows::window_put_char(proc, column, row, ch, color);
}

void draw_text(size_t column, size_t row, const char *text, Color color)
{
    if (text == nullptr)
    {
        return;
    }

    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    size_t cursor_col = column;
    size_t cursor_row = row;
    const size_t max_columns = columns();
    const size_t max_rows = rows();

    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        char ch = text[i];

        if (ch == '\n')
        {
            cursor_col = column;
            if (++cursor_row >= max_rows)
            {
                break;
            }
            continue;
        }

        if (cursor_col >= max_columns)
        {
            cursor_col = column;
            if (++cursor_row >= max_rows)
            {
                break;
            }
        }

        terminal_windows::window_put_char(proc, cursor_col, cursor_row, ch, color);
        ++cursor_col;
    }
}

void fill_rect(size_t column, size_t row, size_t width, size_t height, char ch, Color color)
{
    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    const size_t max_columns = columns();
    const size_t max_rows = rows();

    for (size_t y = 0; y < height; ++y)
    {
        size_t target_row = row + y;
        if (target_row >= max_rows)
        {
            break;
        }

        for (size_t x = 0; x < width; ++x)
        {
            size_t target_col = column + x;
            if (target_col >= max_columns)
            {
                break;
            }

            terminal_windows::window_put_char(proc, target_col, target_row, ch, color);
        }
    }
}

void set_cursor(size_t row, size_t column, bool active)
{
    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    terminal_windows::window_set_cursor(proc, row, column, active);
}

bool get_cursor(size_t &row, size_t &column)
{
    row = 0;
    column = 0;

    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return false;
    }

    return terminal_windows::window_get_cursor(proc, row, column);
}

void present()
{
    Process *proc = current_process();
    if (!ensure_window_for(proc))
    {
        return;
    }

    terminal_windows::window_present(proc);
}

} // namespace graphics

