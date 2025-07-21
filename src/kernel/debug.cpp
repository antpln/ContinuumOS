#include <kernel/debug.h>
#include <stdio.h>
#include <stdarg.h>

void panic(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[PANIC] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    while (1) {
        __asm__("cli; hlt");  // Halt CPU to prevent further execution
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