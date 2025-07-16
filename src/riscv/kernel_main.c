#include <stddef.h>
#include <stdint.h>

static volatile uint8_t* const UART = (uint8_t*)0x10000000;

static void uart_putchar(char c) {
    while ((UART[5] & 0x20) == 0) {}
    UART[0] = c;
}

static void uart_write(const char* s) {
    while (*s) {
        if (*s == '\n') uart_putchar('\r');
        uart_putchar(*s++);
    }
}

void kernel_main(void) {
    uart_write("Hello from ContinuumOS RISC-V\n");
    while (1) {
        __asm__ volatile("wfi");
    }
}
