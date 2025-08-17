#include "kernel/timer.h"
#include "kernel/port_io.h"
#include "kernel/isr.h"
#include "kernel/scheduler.h"
#include <stdio.h>
#include <kernel/debug.h>
#include "kernel/pic.h"

volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency_hz = 0;

// Called on every timer tick (IRQ0)
void timer_handler(registers_t* regs) {
    timer_ticks++;
    // Resume any processes waiting for this tick value
    scheduler_resume_processes_for_event(HookType::TIME_REACHED, timer_ticks);
    scheduler_on_tick(regs);
}

// Initialize the PIT timer to the given frequency.
void init_timer(uint32_t frequency) {
    // Calculate divisor: PIT frequency is 1193180 Hz.
    uint32_t divisor = 1193180 / frequency;

    // Command port 0x43: set PIT to rate generator mode.
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);         // Low byte
    outb(0x40, (divisor >> 8) & 0xFF);    // High byte

    timer_frequency_hz = frequency;

    // Register timer_handler for IRQ0 (interrupt 32).
    register_interrupt_handler(32, timer_handler);

    // Unmask IRQ0 on the PIC so timer interrupts are delivered
    pic_unmask_irq(0);

    success("[TIMER] Timer initialized to %d Hz", frequency);
}

uint32_t get_ticks() {
    return timer_ticks;
}

uint32_t get_ticks_miliseconds() {
    if (timer_frequency_hz == 0) return 0;
    // Convert ticks to milliseconds: ticks * (1000 / Hz)
    return (timer_ticks * 1000) / timer_frequency_hz;
}
