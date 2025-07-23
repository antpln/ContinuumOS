#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#include <stdint.h>

// IDT entry structure
struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

// IDT descriptor structure
struct IDTDescriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void init_idt();
void init_syscall_handler();
void idt_set_gate(uint8_t num, uint32_t offset, uint16_t selector, uint8_t flags);
void debug_idt_entry(int i);
#endif