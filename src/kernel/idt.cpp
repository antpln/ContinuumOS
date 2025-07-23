#include <kernel/idt.h>
#include <stdio.h>

#define IDT_ENTRIES 256

static IDTEntry idt[IDT_ENTRIES];
static IDTDescriptor idt_desc;

extern "C" void load_idt(uint32_t);

extern "C" void* isr_stub_table[32];
extern "C" void* irq_stub_table[16];

void init_idt() {
    idt_desc.limit = (sizeof(IDTEntry) * IDT_ENTRIES) - 1;
    idt_desc.base = (uint32_t)&idt;


    // Clear IDT
    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set CPU exceptions (0-31) to their respective handlers
    for (uint8_t i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)isr_stub_table[i], 0x08, 0x8E);  // Interrupt Gate
    }

    // Set IRQs (32-47) to their respective handlers
    for (uint8_t i = 32; i < 48; i++) {
        idt_set_gate(i, (uint32_t)irq_stub_table[i - 32], 0x08, 0x8E);  // Interrupt Gate
    }
    // Load IDT
    load_idt((uint32_t)&idt_desc);
}

void idt_set_gate(uint8_t num, uint32_t offset, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = offset & 0xFFFF;
    idt[num].offset_high = (offset >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags | 0x80; // Ensure Present (P) bit is set

}

extern "C" void _syscall_handler();

void init_syscall_handler() {
    // Set IDT entry for syscall interrupt (0x80)
    idt_set_gate(0x80, (uint32_t)_syscall_handler, 0x08, 0x8E);
}

