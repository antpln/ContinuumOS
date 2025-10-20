#pragma once

#include <stddef.h>
#include <stdint.h>

#include <kernel/vga.h>

namespace graphics
{
using Color = uint8_t;

// Construct a VGA color value compatible with text-mode windows.
Color make_color(vga_color foreground, vga_color background);

size_t columns();
size_t rows();

// Ensure the current process owns a window before drawing.
void ensure_window();

void clear(Color color, char fill_char = ' ');

void put_char(size_t column, size_t row, char ch, Color color);
void draw_text(size_t column, size_t row, const char *text, Color color);
void fill_rect(size_t column, size_t row, size_t width, size_t height, char ch, Color color);

void set_cursor(size_t row, size_t column, bool active);
bool get_cursor(size_t &row, size_t &column);

void present();
} // namespace graphics

