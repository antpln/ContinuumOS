use crate::memory;
use crate::test;

const TEST_PATTERN: u32 = 0xDEADBEEF;

pub fn test_allocation() -> bool {
    if let Some(frame) = memory::allocate_frame() {
        unsafe {
            (frame as *mut u32).write_volatile(TEST_PATTERN);
        }
        let success = unsafe { (frame as *mut u32).read_volatile() } == TEST_PATTERN;
        memory::free_frame(frame);
        success
    } else {
        false
    }
}

pub fn test_free() -> bool {
    if let Some(frame1) = memory::allocate_frame() {
        memory::free_frame(frame1);
        let frame2 = memory::allocate_frame();
        let success = frame2 == Some(frame1);
        if let Some(f) = frame2 {
            memory::free_frame(f);
        }
        success
    } else {
        false
    }
}

pub fn test_multiple_allocations() -> bool {
    const NUM: usize = 10;
    let mut frames = [0usize; NUM];
    for i in 0..NUM {
        if let Some(f) = memory::allocate_frame() {
            unsafe {
                (f as *mut u32).write_volatile(TEST_PATTERN + i as u32);
            }
            frames[i] = f;
        } else {
            for j in 0..i {
                memory::free_frame(frames[j]);
            }
            return false;
        }
    }
    let mut ok = true;
    for i in 0..NUM {
        if unsafe { (frames[i] as *mut u32).read_volatile() } != TEST_PATTERN + i as u32 {
            ok = false;
        }
    }
    for &f in frames.iter() {
        memory::free_frame(f);
    }
    ok
}
