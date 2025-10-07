#include <stdio.h>
#include <kernel/vga.h>
#include <kernel/syscalls.h>
#include <stdarg.h>

extern Terminal terminal; // Access singleton terminal object

int putchar(char c) {
    terminal.putchar(c);
    return c;
}

int puts(const char* str) {
    terminal.writestring(str);
    terminal.writestring("\n");
    return 0;
}

static void itoa(int32_t value, char* buffer, int base) {
    static char digits[] = "0123456789ABCDEF";
    char temp[32];  // Temporary buffer for storing reversed digits
    int i = 0;
    int is_negative = 0;

    if (base == 10 && value < 0) {
        is_negative = 1;
        value = -value;  // Convert to positive for processing
    }

    // Convert the number to string
    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    if (is_negative) {
        temp[i++] = '-';  // Add minus sign for negative numbers
    }

    // Reverse the string into buffer
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';  // Null-terminate the string
}

static void utoa(uint32_t value, char* buffer, int base) {
    static char digits[] = "0123456789ABCDEF";
    char temp[32];
    int i = 0;
    // Convert the number to string
    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);
    // Reverse the string into buffer
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

// Convert integer to string (supports base 10 and 16)
int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[32];

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': { // Signed Integer
                    int num = va_arg(args, int);
                    itoa(num, buffer, 10);
                    terminal.writestring(buffer);
                    break;
                }
                case 'u': { // Unsigned Integer
                    unsigned int num = va_arg(args, unsigned int);
                    utoa(num, buffer, 10);
                    terminal.writestring(buffer);
                    break;
                }
                case 'x': { // Hexadecimal
                    unsigned int num = va_arg(args, unsigned int);
                    itoa((int32_t)num, buffer, 16);
                    terminal.writestring(buffer);
                    break;
                }
                case 's': { // String
                    char* str = va_arg(args, char*);
                    terminal.writestring(str);
                    break;
                }
                case 'c': { // Character
                    char c = (char)va_arg(args, int);
                    terminal.putchar(c);
                    break;
                }
                case 'p': { // Pointer
                    void* ptr = va_arg(args, void*);
                    itoa((uintptr_t)ptr, buffer, 16);
                    terminal.writestring(buffer);
                    break;
                }
                case '%': { // Literal '%'
                    terminal.putchar('%');
                    break;
                }
                default:
                    terminal.putchar('%');
                    terminal.putchar(*format);
            }
        } else {
            terminal.putchar(*format);
        }
        format++;
    }

    va_end(args);
    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
int vprintf(const char* format, va_list args) {
    char buffer[32];
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int num = va_arg(args, int);
                    itoa(num, buffer, 10);
                    terminal.writestring(buffer);
                    break;
                }
                case 'u': {
                    unsigned int num = va_arg(args, unsigned int);
                    utoa(num, buffer, 10);
                    terminal.writestring(buffer);
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    itoa((int32_t)num, buffer, 16);
                    terminal.writestring(buffer);
                    break;
                }
                case 's': {
                    char* str = va_arg(args, char*);
                    terminal.writestring(str);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    terminal.putchar(c);
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(args, void*);
                    itoa((uintptr_t)ptr, buffer, 16);
                    terminal.writestring(buffer);
                    break;
                }
                case '%': {
                    terminal.putchar('%');
                    break;
                }
                default:
                    terminal.putchar('%');
                    terminal.putchar(*format);
            }
        } else {
            terminal.putchar(*format);
        }
        format++;
    }
    return 0;
}
#ifdef __cplusplus
}
#endif

// File I/O wrappers
int open(const char* path) {
    return sys_open(path);
}

int read(int fd, uint8_t* buffer, size_t size) {
    return sys_read(fd, buffer, size);
}

int write(int fd, const uint8_t* buffer, size_t size) {
    return sys_write(fd, buffer, size);
}

void close(int fd) {
    sys_close(fd);
}

// Keyboard input wrapper
int getchar() {
    return sys_getchar();
}