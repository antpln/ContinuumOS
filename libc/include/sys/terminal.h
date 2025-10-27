#ifndef LIBC_SYS_TERMINAL_H
#define LIBC_SYS_TERMINAL_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/vga.h>
#include <sys/syscall.h>

static inline uint8_t terminal_make_color(vga_color foreground, vga_color background)
{
    return syscall_terminal_make_color((uint32_t)foreground, (uint32_t)background);
}

static inline void terminal_put_at(char ch, uint8_t color, size_t column, size_t row)
{
    syscall_terminal_put_at(ch, color, column, row);
}

static inline void terminal_set_cursor(size_t row, size_t column)
{
    syscall_terminal_set_cursor(row, column);
}

#endif // LIBC_SYS_TERMINAL_H
