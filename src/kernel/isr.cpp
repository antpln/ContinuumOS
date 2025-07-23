#include <kernel/isr.h>
#include <kernel/pic.h>
#include <stdio.h>
#include <kernel/port_io.h>
#include <kernel/debug.h>

#define ISR_COUNT 256 // Total number of ISRs

// Array of function pointers to handle interrupts
static isr_t interrupt_handlers[ISR_COUNT];

// Registers a custom ISR handler for a given interrupt
void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

// ISR Handler (for CPU exceptions)
extern "C" void isr_handler(registers_t *regs)
{
    // Print the interrupt code (exception number)
    error("ISR Exception: Interrupt %d, Error Code: %d", regs->int_no, regs->err_code);

    // Handle critical exceptions like page faults (Interrupt 14)
    if(interrupt_handlers[regs->int_no]) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }

    // Halt if it's a critical CPU exception
    if (regs->int_no < 32)
    {
        PANIC("Unhandled CPU exception %d", regs->int_no);
    }
}

// IRQ Handler (for hardware interrupts)
extern "C" void irq_handler(registers_t *regs)
{
    // Send an EOI (end of interrupt) signal to the PICs
    uint32_t irq_nb = regs->int_no - 32;

    if (regs->int_no >= 40)
    {
        // Send reset signal to slave PIC
        outb(0xA0, 0x20);
    }
    if (regs->int_no >= 32)
    {
        // Send reset signal to master PIC
        outb(0x20, 0x20);
    }

    if (interrupt_handlers[regs->int_no])
    {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
}
