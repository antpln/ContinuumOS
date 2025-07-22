use crate::port_io::outb;
use crate::isr::{self, Registers};
use crate::pic;

static mut TICKS: u32 = 0;

pub fn timer_handler(_regs: &mut Registers) {
    unsafe {
        TICKS += 1;
    }
}

pub unsafe fn init_timer(freq: u32) {
    let divisor: u32 = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (divisor & 0xFF) as u8);
    outb(0x40, ((divisor >> 8) & 0xFF) as u8);
    isr::register_interrupt_handler(32, timer_handler);
    pic::pic_unmask_irq(0);
}

pub fn ticks() -> u32 {
    unsafe { TICKS }
}
