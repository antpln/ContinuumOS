use crate::ramfs::{fs_open, fs_read, fs_write, fs_close, FSNode};

#[repr(u32)]
pub enum Syscall {
    Open = 0,
    Read = 1,
    Write = 2,
    Close = 3,
}

pub unsafe fn syscall(num: u32, arg1: usize, arg2: usize, arg3: usize) -> isize {
    match num {
        x if x == Syscall::Open as u32 => fs_open(arg1 as *mut FSNode) as isize,
        x if x == Syscall::Read as u32 => fs_read(arg1 as i32, arg2 as *mut u8, arg3) as isize,
        x if x == Syscall::Write as u32 => fs_write(arg1 as i32, arg2 as *const u8, arg3) as isize,
        x if x == Syscall::Close as u32 => { fs_close(arg1 as i32); 0 },
        _ => -1,
    }
}

