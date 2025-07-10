#ifndef _KERNEL_VGA_H
#define _KERNEL_VGA_H

#include <stdint.h>
#include <stddef.h>

typedef enum vga_color {
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

class Terminal {
private:
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;

    size_t row;
    size_t column;
    uint8_t color;
    uint16_t* buffer;

    uint16_t make_entry(unsigned char uc, uint8_t color);
    void putentry_at(char c, uint8_t color, size_t x, size_t y);
    void new_line();
    void scroll();

public:
    Terminal();

    void initialize();
    void clear();

    void setcolor(uint8_t color);
    void setfull_color(enum vga_color fg, enum vga_color bg);
    void writestring(const char* str);
    void writeLine(const char* str);
    void putchar(char c);
    uint8_t make_color(enum vga_color fg, enum vga_color bg);

    void update_cursor();
    void set_cursor(size_t r, size_t c);

    void put_at(char c, uint8_t color, size_t x, size_t y);

    size_t get_vga_height() const;
    size_t get_vga_width() const;
};

#endif
