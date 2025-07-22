use core::arch::asm;

#[repr(C, packed)]
#[derive(Copy, Clone)]
struct GdtEntry {
    limit_low: u16,
    base_low: u16,
    base_mid: u8,
    access: u8,
    granularity: u8,
    base_high: u8,
}

#[repr(C, packed)]
struct GdtPtr {
    limit: u16,
    base: u32,
}

static mut GDT: [GdtEntry; 5] = [GdtEntry { limit_low: 0, base_low: 0, base_mid: 0, access: 0, granularity: 0, base_high: 0 }; 5];

fn set_entry(i: usize, base: u32, limit: u32, access: u8, gran: u8) {
    unsafe {
        GDT[i].limit_low = (limit & 0xFFFF) as u16;
        GDT[i].granularity = ((limit >> 16) & 0x0F) as u8;
        GDT[i].base_low = (base & 0xFFFF) as u16;
        GDT[i].base_mid = ((base >> 16) & 0xFF) as u8;
        GDT[i].base_high = ((base >> 24) & 0xFF) as u8;
        GDT[i].access = access;
        GDT[i].granularity |= gran & 0xF0;
    }
}

unsafe fn load_gdt(gdt_ptr: &GdtPtr) {
    asm!(
        "lgdt [{}]",
        in(reg) gdt_ptr,
        options(nostack, preserves_flags)
    );
    asm!(
        "mov ax, 0x10",
        "mov ds, ax",
        "mov es, ax",
        "mov fs, ax",
        "mov gs, ax",
        "mov ss, ax",
        "push 0x08",
        "push 2f",
        "retf",
        "2:",
        options(nostack)
    );
}

pub unsafe fn init_gdt() {
    set_entry(0, 0, 0, 0, 0);
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);

    let gdt_ptr = GdtPtr {
        limit: (core::mem::size_of_val(&GDT) - 1) as u16,
        base: &GDT as *const _ as u32,
    };

    load_gdt(&gdt_ptr);
}
