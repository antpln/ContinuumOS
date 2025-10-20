#include <kernel/vga.h>
#include <kernel/port_io.h>
#include <kernel/framebuffer.h>
#include <kernel/font8x16.h>
#include <kernel/gui.h>

#include <string.h>

namespace
{
struct RGB
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

constexpr RGB VGA_PALETTE[16] = {
	{0, 0, 0},			// black
	{0, 0, 170},		// blue
	{0, 170, 0},		// green
	{0, 170, 170},		// cyan
	{170, 0, 0},		// red
	{170, 0, 170},		// magenta
	{170, 85, 0},		// brown
	{170, 170, 170},	// light grey
	{85, 85, 85},		// dark grey
	{85, 85, 255},		// light blue
	{85, 255, 85},		// light green
	{85, 255, 255},		// light cyan
	{255, 85, 85},		// light red
	{255, 85, 255},		// light magenta
	{255, 255, 85},		// yellow
	{255, 255, 255}		// white
};

constexpr char FALLBACK_GLYPH = '?';
} // namespace

Terminal::Terminal()
	: row(0),
	  column(0),
	  color(0),
	  buffer(nullptr),
	  cells{},
	  framebuffer_mode(false),
	  cursor_row(0),
	  cursor_column(0),
	  cursor_active(false),
	  palette_cache{},
	  origin_x_px(0),
	  origin_y_px(0)
{
}

uint8_t Terminal::make_color(enum vga_color fg, enum vga_color bg)
{
	return fg | (bg << 4);
}

uint16_t Terminal::make_entry(unsigned char uc, uint8_t entry_color)
{
	return (uint16_t)uc | ((uint16_t)entry_color << 8);
}

void Terminal::initialize()
{
	row = 0;
	column = 0;
	cursor_row = 0;
	cursor_column = 0;
	cursor_active = false;
	color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	buffer = (uint16_t *)0xB8000;
	framebuffer_mode = framebuffer::is_available();

	if (framebuffer_mode)
	{
		for (size_t i = 0; i < 16; ++i)
		{
			palette_cache[i] = framebuffer::pack_color(VGA_PALETTE[i].r, VGA_PALETTE[i].g, VGA_PALETTE[i].b);
		}
	}

	memset(cells, 0, sizeof(cells));

	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			cells[y][x].character = ' ';
			cells[y][x].color = color;
			render_cell(x, y);
		}
	}

	if (framebuffer_mode)
	{
		framebuffer::present();
	}

	update_cursor();
}

void Terminal::putentry_at(char c, uint8_t entry_color, size_t x, size_t y)
{
	if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
	{
		return;
	}

	cells[y][x].character = c;
	cells[y][x].color = entry_color;

	if (!framebuffer_mode)
	{
		const size_t index = y * VGA_WIDTH + x;
		buffer[index] = make_entry(c, entry_color);
	}
	else
	{
		render_cell(x, y);
	}
}

void Terminal::put_at(char c, uint8_t entry_color, size_t x, size_t y)
{
	putentry_at(c, entry_color, x, y);
}

void Terminal::erase_cursor()
{
	if (!framebuffer_mode || !cursor_active)
	{
		return;
	}

	if (cursor_column < VGA_WIDTH && cursor_row < VGA_HEIGHT)
	{
		render_cell(cursor_column, cursor_row);
	}

	cursor_active = false;
}

void Terminal::draw_cursor()
{
	if (!framebuffer_mode)
	{
		return;
	}

	if (cursor_column >= VGA_WIDTH || cursor_row >= VGA_HEIGHT)
	{
		return;
	}

	const uint32_t px = static_cast<uint32_t>(origin_x_px + cursor_column * gui::FONT_WIDTH);
	const uint32_t py = static_cast<uint32_t>(origin_y_px + cursor_row * gui::FONT_HEIGHT);

	const uint32_t caret_color = framebuffer::pack_color(240, 240, 255);
	framebuffer::fill_rect(px,
						   py,
						   2,
						   gui::FONT_HEIGHT,
						   caret_color,
						   framebuffer::BufferTarget::Display);
	cursor_active = true;
}

void Terminal::update_cursor()
{
	if (framebuffer_mode)
	{
		erase_cursor();
		framebuffer::present();
		cursor_row = row;
		cursor_column = column;
		draw_cursor();
		return;
	}

	uint16_t pos = row * VGA_WIDTH + column;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void Terminal::set_cursor(size_t r, size_t c)
{
	if (r >= VGA_HEIGHT)
	{
		r = VGA_HEIGHT - 1;
	}
	if (c >= VGA_WIDTH)
	{
		c = VGA_WIDTH - 1;
	}

	row = r;
	column = c;
	update_cursor();
}

void Terminal::set_graphics_origin(size_t x, size_t y, bool refresh_now)
{
	origin_x_px = x;
	origin_y_px = y;
	if (framebuffer_mode && refresh_now)
	{
		refresh();
	}
}

size_t Terminal::get_graphics_origin_x() const
{
	return origin_x_px;
}

size_t Terminal::get_graphics_origin_y() const
{
	return origin_y_px;
}

size_t Terminal::pixel_width() const
{
	return VGA_WIDTH * gui::FONT_WIDTH;
}

size_t Terminal::pixel_height() const
{
	return VGA_HEIGHT * gui::FONT_HEIGHT;
}

bool Terminal::is_framebuffer_enabled() const
{
	return framebuffer_mode;
}

void Terminal::refresh()
{
	if (!framebuffer_mode)
	{
		return;
	}

	erase_cursor();
	redraw_all();
	update_cursor();
}

void Terminal::save_snapshot(Snapshot &out) const
{
	for (size_t y = 0; y < VGA_HEIGHT; ++y)
	{
		for (size_t x = 0; x < VGA_WIDTH; ++x)
		{
			out.characters[y][x] = cells[y][x].character;
			out.colors[y][x] = cells[y][x].color;
		}
	}
	out.row = row;
	out.column = column;
	out.color = color;
	out.cursor_row = cursor_row;
	out.cursor_column = cursor_column;
	out.cursor_active = cursor_active;
}

void Terminal::load_snapshot(const Snapshot &snapshot)
{
	erase_cursor();
	for (size_t y = 0; y < VGA_HEIGHT; ++y)
	{
		for (size_t x = 0; x < VGA_WIDTH; ++x)
		{
			cells[y][x].character = snapshot.characters[y][x];
			cells[y][x].color = snapshot.colors[y][x];
		}
	}
	row = snapshot.row;
	column = snapshot.column;
	color = snapshot.color;
	cursor_row = snapshot.cursor_row;
	cursor_column = snapshot.cursor_column;
	cursor_active = snapshot.cursor_active;
	if (framebuffer_mode)
	{
		redraw_all();
		update_cursor();
	}
}

void Terminal::new_line()
{
	column = 0;
	if (++row == VGA_HEIGHT)
	{
		scroll();
		row = VGA_HEIGHT - 1;
	}
	update_cursor();
}

void Terminal::scroll()
{
	erase_cursor();

	for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			cells[y][x] = cells[y + 1][x];
			if (!framebuffer_mode)
			{
				const size_t index = y * VGA_WIDTH + x;
				buffer[index] = make_entry(cells[y][x].character, cells[y][x].color);
			}
		}
	}

	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		cells[VGA_HEIGHT - 1][x].character = ' ';
		cells[VGA_HEIGHT - 1][x].color = color;
		if (!framebuffer_mode)
		{
			const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
			buffer[index] = make_entry(' ', color);
		}
	}

	if (framebuffer_mode)
	{
		// Redraw the entire grid to reflect the scroll operation.
		redraw_all();
	}

	update_cursor();
}

void Terminal::putchar(char c)
{
	if (c == '\b')
	{
		if (column > 0)
		{
			column--;
		}
		else if (row > 0)
		{
			row--;
			column = VGA_WIDTH - 1;
		}

		putentry_at(' ', color, column, row);
		update_cursor();
		return;
	}

	if (c == '\n')
	{
		new_line();
		return;
	}

	putentry_at(c, color, column, row);
	if (++column == VGA_WIDTH)
	{
		new_line();
	}
	else
	{
		update_cursor();
	}
}

void Terminal::setcolor(uint8_t new_color)
{
	color = new_color;
}

void Terminal::setfull_color(enum vga_color fg, enum vga_color bg)
{
	color = make_color(fg, bg);
}

void Terminal::writestring(const char *str)
{
	while (*str)
	{
		putchar(*str++);
	}
}

void Terminal::writeLine(const char *str)
{
	writestring(str);
	new_line();
}

void Terminal::clear()
{
	erase_cursor();

	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			cells[y][x].character = ' ';
			cells[y][x].color = color;
			if (!framebuffer_mode)
			{
				buffer[y * VGA_WIDTH + x] = make_entry(' ', color);
			}
		}
	}

	if (framebuffer_mode)
	{
		redraw_all();
	}

	row = 0;
	column = 0;
	update_cursor();
}

size_t Terminal::get_vga_height() const
{
	return VGA_HEIGHT;
}

size_t Terminal::get_vga_width() const
{
	return VGA_WIDTH;
}

void Terminal::render_cell(size_t x, size_t y)
{
	if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
	{
		return;
	}

	const Cell &cell = cells[y][x];

	if (!framebuffer_mode)
	{
		const size_t index = y * VGA_WIDTH + x;
		buffer[index] = make_entry(cell.character, cell.color);
		return;
	}

	const uint32_t fg = vga_color_to_rgb(cell.color & 0x0F);
	const uint8_t bg_index = static_cast<uint8_t>((cell.color >> 4) & 0x0F);
	const uint32_t bg = vga_color_to_rgb(bg_index);
	const uint32_t px = static_cast<uint32_t>(origin_x_px + x * gui::FONT_WIDTH);
	const uint32_t py = static_cast<uint32_t>(origin_y_px + y * gui::FONT_HEIGHT);

	if (bg_index == VGA_COLOR_BLACK)
	{
		gui::fill_background_rect(px, py, gui::FONT_WIDTH, gui::FONT_HEIGHT);
	}
	else
	{
		framebuffer::fill_rect(px, py, gui::FONT_WIDTH, gui::FONT_HEIGHT, bg);
	}

	char glyph_char = cell.character;
	if (glyph_char < 32 || glyph_char > 126)
	{
		glyph_char = FALLBACK_GLYPH;
	}

	const uint8_t *glyph_rows = gui::glyph_for(glyph_char);
	framebuffer::draw_mono_bitmap(px,
							  py,
							  gui::FONT_WIDTH,
							  gui::FONT_HEIGHT,
							  glyph_rows,
							  1,
							  fg,
							  0,
							  true);
}

void Terminal::redraw_all()
{
	for (size_t y = 0; y < VGA_HEIGHT; ++y)
	{
		for (size_t x = 0; x < VGA_WIDTH; ++x)
		{
			render_cell(x, y);
		}
	}

	// Restore cursor overlay after full redraw.
	cursor_active = false;

	if (framebuffer_mode)
	{
		framebuffer::present();
	}
}

uint32_t Terminal::vga_color_to_rgb(uint8_t vga_color_value) const
{
	if (!framebuffer_mode)
	{
		return 0;
	}

	uint8_t index = vga_color_value & 0x0F;
	return palette_cache[index];
}
