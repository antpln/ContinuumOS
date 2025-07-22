use core::arch::asm;

const PAGE_SIZE: usize = 4096;
const ENTRIES: usize = 1024;

static mut PAGE_DIRECTORY: [u32; ENTRIES] = [0; ENTRIES];
static mut PAGE_TABLES: [[u32; ENTRIES]; 4] = [[0; ENTRIES]; 4];

pub unsafe fn vmm_init() {
    for table in 0..4 {
        for i in 0..ENTRIES {
            let addr = (table as u32 * 0x400000) + (i as u32 * PAGE_SIZE as u32);
            PAGE_TABLES[table][i] = (addr & 0xFFFFF000) | 0x03;
        }
        PAGE_DIRECTORY[table] = (&PAGE_TABLES[table] as *const _ as u32) | 0x03;
    }
}

pub unsafe fn vmm_enable() {
    asm!("mov cr3, {}", in(reg) &PAGE_DIRECTORY as *const _ as u32, options(nostack));
    let mut cr0: u32;
    asm!("mov {}, cr0", out(reg) cr0);
    cr0 |= 0x8000_0000;
    asm!("mov cr0, {}", in(reg) cr0, options(nostack));
}

pub unsafe fn vmm_map(virtual_addr: u32, physical_addr: u32, rw: bool) {
    let pd_index = (virtual_addr >> 22) as usize;
    let pt_index = ((virtual_addr >> 12) & 0x3FF) as usize;
    let pt_ptr = (PAGE_DIRECTORY[pd_index] & 0xFFFFF000) as *mut u32;
    let flags = if rw {0x3} else {0x1};
    pt_ptr.add(pt_index).write_volatile((physical_addr & 0xFFFFF000) | flags);
    asm!("invlpg [{}]", in(reg) virtual_addr, options(nostack));
}
