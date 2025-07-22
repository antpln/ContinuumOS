use core::arch::asm;

/// Write a byte to an I/O port
pub unsafe fn outb(port: u16, val: u8) {
    asm!("out dx, al", in("dx") port, in("al") val, options(nomem, nostack, preserves_flags));
}

/// Read a byte from an I/O port
pub unsafe fn inb(port: u16) -> u8 {
    let mut val: u8;
    asm!("in al, dx", out("al") val, in("dx") port, options(nomem, nostack));
    val
}

/// Read a word from an I/O port
pub unsafe fn inw(port: u16) -> u16 {
    let mut val: u16;
    asm!("in ax, dx", out("ax") val, in("dx") port, options(nomem, nostack));
    val
}

/// Wait for an I/O operation to complete
pub unsafe fn io_wait() {
    asm!("out 0x80, al", in("al") 0u8, options(nomem, nostack));
}
