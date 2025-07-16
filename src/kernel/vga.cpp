#include <kernel/vga.h>
#include <kernel/port_io.h>
#if defined(__riscv)
#include <stdint.h>
#endif

Terminal::Terminal() : row(0), column(0), color(0), buffer(nullptr) {}

uint8_t Terminal::make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

uint16_t Terminal::make_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | ((uint16_t)color << 8);
}

#if defined(__riscv)
static volatile uint8_t* const UART = (uint8_t*)0x10000000;
static void uart_putchar(char c) {
    while ((UART[5] & 0x20) == 0) {}
    UART[0] = c;
}
#endif

void Terminal::initialize() {
#if defined(__riscv)
    (void)UART; // nothing to init
#else
    row = 0;
    column = 0;
    color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    buffer = (uint16_t*)0xB8000;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = make_entry(' ', color);
        }
    }
#endif
}

void Terminal::putentry_at(char c, uint8_t color, size_t x, size_t y) {
#if defined(__riscv)
    (void)color; (void)x; (void)y; uart_putchar(c);
#else
    const size_t index = y * VGA_WIDTH + x;
    buffer[index] = make_entry(c, color);
#endif
}

void Terminal::put_at(char c, uint8_t color, size_t x, size_t y) {
    putentry_at(c, color, x, y);
}

void Terminal::update_cursor() {
#if defined(__riscv)
    (void)row; (void)column;
#else
    uint16_t pos = row * VGA_WIDTH + column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
#endif
}

void Terminal::set_cursor(size_t r, size_t c) {
    row = r;
    column = c;
    update_cursor();
}

void Terminal::new_line() {
#if defined(__riscv)
    uart_putchar('\n');
#else
    column = 0;
    if (++row == VGA_HEIGHT) {
        scroll();
        row = VGA_HEIGHT - 1;
    }
    update_cursor();
#endif
}

void Terminal::scroll() {
#if defined(__riscv)
    (void)row; (void)column;
#else
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', color);
    }
#endif
}

void Terminal::putchar(char c) {
#if defined(__riscv)
    if (c == '\n') {
        uart_putchar('\r');
        uart_putchar('\n');
        return;
    }
    uart_putchar(c);
    return;
#endif
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
#if defined(__riscv)
    (void)new_color;
#else
    color = new_color;
#endif
}

void Terminal::setfull_color(enum vga_color fg, enum vga_color bg) {
    color = make_color(fg, bg);
}

void Terminal::writestring(const char* str) {
#if defined(__riscv)
    while (*str) {
        putchar(*str++);
    }
    return;
#else
    while (*str) {
        putchar(*str++);
    }
#endif
}

void Terminal::writeLine(const char* str) {
    writestring(str);
    new_line();
}

void Terminal::clear() {
#if defined(__riscv)
    (void)color; row = column = 0;
#else
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            buffer[y * VGA_WIDTH + x] = make_entry(' ', color);
        }
    }
    row = 0;
    column = 0;
    update_cursor();
#endif
}

size_t Terminal::get_vga_height() const {
#if defined(__riscv)
    return 0;
#else
    return VGA_HEIGHT;
#endif
}

size_t Terminal::get_vga_width() const {
#if defined(__riscv)
    return 0;
#else
    return VGA_WIDTH;
#endif
}
