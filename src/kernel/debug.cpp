#include <kernel/debug.h>
#include <stdio.h>
#include <stdarg.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdbool.h>


extern Terminal terminal;

static bool format_has_placeholders(const char* fmt) {
    if (!fmt) {
        return false;
    }

    while (*fmt) {
        if (*fmt == '%') {
            ++fmt;
            if (*fmt == '%') {
                ++fmt;
                continue;
            }
            return true;
        }
        ++fmt;
    }
    return false;
}

void panic(const char* msg, const char* file, int line, const char* func, ...) {
    va_list args;
    va_start(args, func);
    va_list serial_args;
    va_copy(serial_args, args);
    terminal.setfull_color(VGA_COLOR_BLACK, VGA_COLOR_RED);
    terminal.clear();
    // Sad face ASCII art
    printf("\n\n        :(\n");
    printf("\n================ KERNEL PANIC ================\n");
    printf("A critical error occurred and the kernel must stop.\n\n");
    printf("Message: %s\n", msg);
    printf("Location: %s:%d\n", file, line);
    printf("Function: %s\n", func);
    const bool has_details = format_has_placeholders(msg);
    if (has_details) {
        printf("Details: ");
        vprintf(msg, args);
        printf("\n");
    }
    printf("\n==============================================\n");
    serial_write("\n\n        :(\n");
    serial_write("\n================ KERNEL PANIC ================\n");
    serial_write("A critical error occurred and the kernel must stop.\n\n");
    serial_printf("Message: %s\n", msg);
    serial_printf("Location: %s:%d\n", file, line);
    serial_printf("Function: %s\n", func);
    if (has_details) {
        serial_write("Details: ");
        serial_vprintf(msg, serial_args);
        serial_write("\n");
    }
    serial_write("\n==============================================\n");
    va_end(serial_args);
    va_end(args);
    while (1) {
        __asm__("cli; hlt");
    }
}
void debug(const char* fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[DEBUG] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[DEBUG] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
void success(const char* fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[SUCCESS] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[SUCCESS] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[ERROR] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[ERROR] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
}
void test(const char* fmt, ...) {
#ifdef TEST
    va_list args;
    va_start(args, fmt);
    va_list serial_args;
    va_copy(serial_args, args);
    printf("[TEST] ");
    vprintf(fmt, args);
    printf("\n");
    serial_write("[TEST] ");
    serial_vprintf(fmt, serial_args);
    serial_write("\n");
    va_end(serial_args);
    va_end(args);
#else
    (void)fmt;
#endif
}
