use crate::port_io::{inb, outb, io_wait};

const PIC1: u16 = 0x20;
const PIC2: u16 = 0xA0;
const PIC1_COMMAND: u16 = PIC1;
const PIC1_DATA: u16 = PIC1 + 1;
const PIC2_COMMAND: u16 = PIC2;
const PIC2_DATA: u16 = PIC2 + 1;
const PIC_EOI: u8 = 0x20;

/// Remap the PIC to avoid conflicts with CPU exceptions.
pub unsafe fn pic_remap() {
    let a1 = inb(PIC1_DATA);
    let a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, a1); io_wait();
    outb(PIC2_DATA, a2); io_wait();
}

/// Send End Of Interrupt (EOI) signal for the given IRQ.
pub unsafe fn pic_send_eoi(irq: u8) {
    if irq >= 8 {
        outb(PIC2_COMMAND, PIC_EOI);
        io_wait();
    }
    outb(PIC1_COMMAND, PIC_EOI);
    io_wait();
}

/// Unmask the specified IRQ line.
pub unsafe fn pic_unmask_irq(irq: u8) {
    let port: u16 = if irq < 8 { 0x21 } else { 0xA1 };
    let mask = if irq < 8 { irq } else { irq - 8 };
    let mut value = inb(port);
    value &= !(1 << mask);
    outb(port, value);
    let check = inb(port);
    if check & (1 << mask) != 0 {
        // Could use a logging facility once available
    }
}

/// Initialize and remap the PIC.
pub unsafe fn init_pic() {
    pic_remap();
}
