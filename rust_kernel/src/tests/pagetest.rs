use crate::test;
use crate::{memory, paging};

pub fn run() {
    test!("Paging Test: Mapping and Unmapping");
    let vaddr: u32 = 0x400000;
    if let Some(frame) = memory::allocate_frame() {
        unsafe {
            paging::vmm_map(vaddr, frame as u32, true);
        }
        test!("Mapped vaddr 0x{:x} to paddr 0x{:x}", vaddr, frame);
        unsafe {
            paging::vmm_map(vaddr, 0, true);
        }
        test!("Unmapped vaddr 0x{:x}", vaddr);
        memory::free_frame(frame);
        test!("Paging Test: Completed");
    } else {
        panic!("Paging Test: Failed to allocate frame");
    }
}
