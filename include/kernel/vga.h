#ifndef _KERNEL_VGA_H
#define _KERNEL_VGA_H

#include <stdint.h>
#include <stddef.h>

typedef enum vga_color
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
} vga_color;

class Terminal
{
  public:
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;

    struct Snapshot
    {
        char characters[VGA_HEIGHT][VGA_WIDTH];
        uint8_t colors[VGA_HEIGHT][VGA_WIDTH];
        size_t row;
        size_t column;
        uint8_t color;
        size_t cursor_row;
        size_t cursor_column;
        bool cursor_active;
    };

    Terminal();

    void initialize();
    void clear();

    void setcolor(uint8_t color);
    void setfull_color(enum vga_color fg, enum vga_color bg);
    void writestring(const char *str);
    void writeLine(const char *str);
    void putchar(char c);
    uint8_t make_color(enum vga_color fg, enum vga_color bg);

    void update_cursor();
    void set_cursor(size_t r, size_t c);

    void put_at(char c, uint8_t color, size_t x, size_t y);

  void set_graphics_origin(size_t x, size_t y, bool refresh_now = true);
    size_t get_graphics_origin_x() const;
    size_t get_graphics_origin_y() const;
    size_t pixel_width() const;
    size_t pixel_height() const;
    bool is_framebuffer_enabled() const;
    void refresh();

    void save_snapshot(Snapshot &out) const;
    void load_snapshot(const Snapshot &snapshot);

    size_t get_vga_height() const;
    size_t get_vga_width() const;

  private:
    struct Cell
    {
        char character;
        uint8_t color;
    };

    size_t row;
    size_t column;
    uint8_t color;
    uint16_t *buffer;
    Cell cells[VGA_HEIGHT][VGA_WIDTH];
    bool framebuffer_mode;
    size_t cursor_row;
    size_t cursor_column;
    bool cursor_active;
    uint32_t palette_cache[16];
    size_t origin_x_px;
    size_t origin_y_px;

    uint16_t make_entry(unsigned char uc, uint8_t color);
    void putentry_at(char c, uint8_t color, size_t x, size_t y);
    void new_line();
    void scroll();
    void erase_cursor();
    void draw_cursor();
    void render_cell(size_t x, size_t y);
    void redraw_all();
    uint32_t vga_color_to_rgb(uint8_t vga_color_value) const;
};

#endif
