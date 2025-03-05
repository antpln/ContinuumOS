#include <stdio.h>
#include <kernel/vga.h>
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
                    itoa((int32_t)num, buffer, 10);
                    terminal.writestring(buffer);
                    break;
                }
                case 'x': { // Hexadecimal
                    unsigned int num = va_arg(args, unsigned int);
                    terminal.writestring("0x");
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
                    terminal.writestring("0x");
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