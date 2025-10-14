#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *str);
int serial_printf(const char *fmt, ...);
int serial_vprintf(const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_SERIAL_H
