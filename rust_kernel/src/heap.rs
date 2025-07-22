use core::ptr::null_mut;

const KERNEL_HEAP_START: usize = 0x0080_0000;
const KERNEL_HEAP_SIZE: usize = 0x0080_0000; // 8 MiB

#[repr(C)]
struct Block {
    size: usize,
    next: *mut Block,
    free: bool,
}

static mut FREE_LIST: *mut Block = null_mut();

pub unsafe fn init_heap() {
    FREE_LIST = KERNEL_HEAP_START as *mut Block;
    (*FREE_LIST).size = KERNEL_HEAP_SIZE - core::mem::size_of::<Block>();
    (*FREE_LIST).next = null_mut();
    (*FREE_LIST).free = true;
}

fn align16(size: usize) -> usize {
    (size + 15) & !15
}

pub unsafe fn kmalloc(size: usize) -> *mut u8 {
    if size == 0 || FREE_LIST.is_null() {
        return null_mut();
    }
    let size = align16(size);
    let mut current = FREE_LIST;
    while !current.is_null() {
        if (*current).free && (*current).size >= size {
            break;
        }
        current = (*current).next;
    }
    if current.is_null() {
        return null_mut();
    }
    if (*current).size >= size + core::mem::size_of::<Block>() + 16 {
        let new_addr = (current as usize) + core::mem::size_of::<Block>() + size;
        let new_block = new_addr as *mut Block;
        (*new_block).size = (*current).size - size - core::mem::size_of::<Block>();
        (*new_block).next = (*current).next;
        (*new_block).free = true;
        (*current).size = size;
        (*current).next = new_block;
    }
    (*current).free = false;
    (current as usize + core::mem::size_of::<Block>()) as *mut u8
}

pub unsafe fn kfree(ptr: *mut u8) {
    if ptr.is_null() { return; }
    let block = (ptr as usize - core::mem::size_of::<Block>()) as *mut Block;
    (*block).free = true;
    if !(*block).next.is_null() && (*(*block).next).free {
        (*block).size += (*(*block).next).size + core::mem::size_of::<Block>();
        (*block).next = (*(*block).next).next;
    }
}

