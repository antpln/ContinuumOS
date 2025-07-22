
const PAGE_SIZE: usize = 4096;
const MEMORY_SIZE: usize = 16 * 1024 * 1024; // 16 MiB
const NUM_FRAMES: usize = MEMORY_SIZE / PAGE_SIZE;
const BITMAP_SIZE: usize = NUM_FRAMES / 32;

static mut BITMAP: [u32; BITMAP_SIZE] = [0; BITMAP_SIZE];
static mut USED_FRAMES: usize = 0;

pub fn initialize() {
    unsafe {
        for b in &mut BITMAP {
            *b = 0;
        }
        USED_FRAMES = 0;
    }
}

pub fn allocate_frame() -> Option<usize> {
    unsafe {
        let frame = first_free();
        if frame == u32::MAX {
            return None;
        }
        set_frame(frame);
        USED_FRAMES += 1;
        Some(frame as usize * PAGE_SIZE)
    }
}

pub fn free_frame(addr: usize) {
    unsafe {
        clear_frame((addr / PAGE_SIZE) as u32);
        if USED_FRAMES > 0 { USED_FRAMES -= 1; }
    }
}

pub fn memory_size() -> usize {
    MEMORY_SIZE
}

pub fn free_frames() -> usize {
    unsafe { NUM_FRAMES - USED_FRAMES }
}

fn set_frame(frame: u32) {
    unsafe {
        let bit = frame % 32;
        let idx = (frame / 32) as usize;
        BITMAP[idx] |= 1 << bit;
    }
}

fn clear_frame(frame: u32) {
    unsafe {
        let bit = frame % 32;
        let idx = (frame / 32) as usize;
        BITMAP[idx] &= !(1 << bit);
    }
}

fn test_frame(frame: u32) -> bool {
    unsafe {
        let bit = frame % 32;
        let idx = (frame / 32) as usize;
        (BITMAP[idx] & (1 << bit)) != 0
    }
}

fn first_free() -> u32 {
    unsafe {
        for (i, val) in BITMAP.iter().enumerate() {
            if *val != 0xFFFF_FFFF {
                for j in 0..32 {
                    let mask = 1 << j;
                    if (*val & mask) == 0 {
                        let frame = (i * 32 + j) as u32;
                        if frame < NUM_FRAMES as u32 {
                            return frame;
                        }
                    }
                }
            }
        }
    }
    u32::MAX
}
