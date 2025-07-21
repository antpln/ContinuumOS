#include <stdint.h>
#include "stdio.h"
#include "kernel/port_io.h"
#include "kernel/pic.h" // Include the new header file
#include <kernel/debug.h>

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20  // End of interrupt command

void pic_remap() {
    uint8_t a1, a2;

    a1 = inb(PIC1_DATA); // Save masks
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);io_wait(); // Start initialization in cascade mode
    outb(PIC2_COMMAND, 0x11);io_wait();
    outb(PIC1_DATA, 0x20);io_wait();    // Master PIC vector offset (IRQ0 = 0x20)
    outb(PIC2_DATA, 0x28);io_wait();    // Slave PIC vector offset (IRQ8 = 0x28)
    outb(PIC1_DATA, 0x04);io_wait();    // Tell Master PIC about Slave at IRQ2
    outb(PIC2_DATA, 0x02);io_wait();    // Tell Slave PIC its cascade identity
    outb(PIC1_DATA, 0x01);io_wait();    // Set 8086/88 (MCS-80/85) mode
    outb(PIC2_DATA, 0x01);io_wait();
    
    // Restore saved masks
    outb(PIC1_DATA, a1);io_wait();
    outb(PIC2_DATA, a2);io_wait();
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
        io_wait();
    }
    outb(PIC1_COMMAND, PIC_EOI);
    io_wait();
}


void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21; // Master PIC data port
    } else {
        port = 0xA1; // Slave PIC data port
        irq -= 8;
    }

    value = inb(port);

    value &= ~(1 << irq); // Clear the IRQ bit to unmask it
    outb(port, value);

    // Read back to confirm it worked
    value = inb(port);

    if (value & (1 << irq)) {
        error("[PIC] IRQ%d is STILL masked!", irq);
    }
}

void init_pic() {
    pic_remap();
    success("[PIC] PIC initialized");
}
