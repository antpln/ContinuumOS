#include <kernel/port_io.h>

// Output a byte to a specified port
void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %1, %0" :: "dN"(port), "a"(value));
}

// Input a byte from a specified port
uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// Input a word (2 bytes) from a specified port
uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void io_wait(void) {
    // A simple way to enforce I/O wait on older hardware. On newer systems/QEMU,
    // it might not be strictly necessary, but it's traditional.
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}
