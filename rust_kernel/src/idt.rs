use core::mem::size_of;
use core::arch::asm;

#[repr(C, packed)]
#[derive(Copy, Clone)]
struct IDTEntry {
    offset_low: u16,
    selector: u16,
    zero: u8,
    type_attr: u8,
    offset_high: u16,
}

#[repr(C, packed)]
struct IDTDescriptor {
    limit: u16,
    base: u32,
}

const IDT_ENTRIES: usize = 256;
static mut IDT: [IDTEntry; IDT_ENTRIES] = [IDTEntry { offset_low: 0, selector: 0, zero: 0, type_attr: 0, offset_high: 0 }; IDT_ENTRIES];

unsafe fn load_idt(idt: &IDTDescriptor) {
    asm!("lidt [{}]", in(reg) idt, options(nostack, preserves_flags));
}

pub unsafe fn idt_set_gate(num: u8, offset: u32, selector: u16, flags: u8) {
    IDT[num as usize].offset_low = (offset & 0xFFFF) as u16;
    IDT[num as usize].offset_high = ((offset >> 16) & 0xFFFF) as u16;
    IDT[num as usize].selector = selector;
    IDT[num as usize].zero = 0;
    IDT[num as usize].type_attr = flags | 0x80;
}

pub unsafe fn init_idt() {
    let idt_desc = IDTDescriptor {
        limit: (size_of::<IDTEntry>() * IDT_ENTRIES - 1) as u16,
        base: &IDT as *const _ as u32,
    };

    for i in 0..IDT_ENTRIES {
        idt_set_gate(i as u8, 0, 0, 0);
    }

    load_idt(&idt_desc);
}
