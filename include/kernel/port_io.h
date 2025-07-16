#ifndef _KERNEL_PORT_IO_H
#define _KERNEL_PORT_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__) || defined(__x86_64__)
// Write a byte to a port
void outb(uint16_t port, uint8_t value);

// Read a byte from a port
uint8_t inb(uint16_t port);

// Read a word (2 bytes) from a port
uint16_t inw(uint16_t port);

void io_wait();
#else
// Stubs for non-x86 architectures
static inline void outb(uint16_t, uint8_t) {}
static inline uint8_t inb(uint16_t) { return 0; }
static inline uint16_t inw(uint16_t) { return 0; }
static inline void io_wait(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_PORT_IO_H
