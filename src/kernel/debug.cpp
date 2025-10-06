#include <kernel/debug.h>
#include <stdio.h>
#include <stdarg.h>
#include <kernel/vga.h>


extern Terminal terminal;
void panic(const char* msg, const char* file, int line, const char* func, ...) {
    va_list args;
    va_start(args, func);
    terminal.setfull_color(VGA_COLOR_BLACK, VGA_COLOR_RED);
    terminal.clear();
    // Sad face ASCII art
    printf("\n\n        :(\n");
    printf("\n================ KERNEL PANIC ================\n");
    printf("A critical error occurred and the kernel must stop.\n\n");
    printf("Message: %s\n", msg);
    printf("Location: %s:%d\n", file, line);
    printf("Function: %s\n", func);
    if (args) {
        printf("Details: ");
        vprintf(msg, args);
        printf("\n");
    printf("\n==============================================\n");
    va_end(args);
    while (1) {
        __asm__("cli; hlt");
    }
}
void debug(const char* fmt, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    printf("[DEBUG] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
#endif
}
void success(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[SUCCESS] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ERROR] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
void test(const char* fmt, ...) {
#ifdef TEST
    va_list args;
    va_start(args, fmt);
    printf("[TEST] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
#endif
}
