#include "kernel/timer.h"
#include "kernel/port_io.h"
#include "kernel/isr.h"
#include <stdio.h>

volatile uint32_t timer_ticks = 0;

// Timer interrupt handler: increments tick count and prints every 100 ticks.
void timer_handler(registers_t* regs) {
    timer_ticks++;
}

// Initialize the PIT timer to the given frequency.
void init_timer(uint32_t frequency) {
    // Calculate divisor: PIT frequency is 1193180 Hz.
    uint32_t divisor = 1193180 / frequency;

    // Command port 0x43: set PIT to rate generator mode.
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);         // Low byte
    outb(0x40, (divisor >> 8) & 0xFF);    // High byte

    // Register timer_handler for IRQ0 (interrupt 32).
    register_interrupt_handler(32, timer_handler);

    printf("[TIMER] Timer initialized to %d Hz\n", frequency);
}

uint32_t get_ticks() {
    return timer_ticks;
}
