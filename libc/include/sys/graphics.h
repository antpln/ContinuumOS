#ifndef LIBC_SYS_GRAPHICS_H
#define LIBC_SYS_GRAPHICS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>

namespace graphics
{
inline void ensure_window()
{
    syscall_graphics_ensure_window();
}

inline void put_char(size_t column, size_t row, char ch, uint8_t color)
{
    syscall_graphics_put_char(column, row, ch, color);
}

inline void present()
{
    syscall_graphics_present();
}

inline void set_cursor(size_t row, size_t column, bool active)
{
    syscall_graphics_set_cursor(row, column, active ? 1 : 0);
}

inline bool get_cursor(size_t &row, size_t &column)
{
    return syscall_graphics_get_cursor(&row, &column) != 0;
}

inline size_t columns()
{
    return syscall_graphics_columns();
}

inline size_t rows()
{
    return syscall_graphics_rows();
}
} // namespace graphics

namespace framebuffer
{
inline bool is_available()
{
    return syscall_framebuffer_is_available() != 0;
}
} // namespace framebuffer

#endif // LIBC_SYS_GRAPHICS_H
