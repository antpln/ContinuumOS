#ifndef _KERNEL_PORT_IO_H
#define _KERNEL_PORT_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Write a byte to a port
void outb(uint16_t port, uint8_t value);

// Write a word (2 bytes) to a port
void outw(uint16_t port, uint16_t value);

// Read a byte from a port
uint8_t inb(uint16_t port);

// Read a word (2 bytes) from a port
uint16_t inw(uint16_t port);

void io_wait();

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_PORT_IO_H
