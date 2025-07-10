#include <kernel/vga.h>
#include <kernel/port_io.h>

Terminal::Terminal() : row(0), column(0), color(0), buffer(nullptr) {}

uint8_t Terminal::make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

uint16_t Terminal::make_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | ((uint16_t)color << 8);
}

void Terminal::initialize() {
    row = 0;
    column = 0;
    color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    buffer = (uint16_t*)0xB8000;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = make_entry(' ', color);
        }
    }
}

void Terminal::putentry_at(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    buffer[index] = make_entry(c, color);
}

void Terminal::put_at(char c, uint8_t color, size_t x, size_t y) {
    putentry_at(c, color, x, y);
}

void Terminal::update_cursor() {
    uint16_t pos = row * VGA_WIDTH + column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void Terminal::set_cursor(size_t r, size_t c) {
    row = r;
    column = c;
    update_cursor();
}

void Terminal::new_line() {
    column = 0;
    if (++row == VGA_HEIGHT) {
        scroll();
        row = VGA_HEIGHT - 1;
    }
    update_cursor();
}

void Terminal::scroll() {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', color);
    }
}

void Terminal::putchar(char c) {
    if (c == '\b') {
        if (column > 0) {
            column--;
        } else if (row > 0) {
            row--;
            column = VGA_WIDTH - 1;
        }
        putentry_at(' ', color, column, row);
        update_cursor();
        return;
    }

    if (c == '\n') {
        new_line();
        return;
    }

    putentry_at(c, color, column, row);
    if (++column == VGA_WIDTH) {
        new_line();
    }
    update_cursor();
}

void Terminal::setcolor(uint8_t new_color) {
    color = new_color;
}

void Terminal::setfull_color(enum vga_color fg, enum vga_color bg) {
    color = make_color(fg, bg);
}

void Terminal::writestring(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

void Terminal::writeLine(const char* str) {
    writestring(str);
    new_line();
}

void Terminal::clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = make_entry(' ', color);
        }
    }
    row = 0;
    column = 0;
    update_cursor();
}

size_t Terminal::get_vga_height() const {
    return VGA_HEIGHT;
}

size_t Terminal::get_vga_width() const {
    return VGA_WIDTH;
}
