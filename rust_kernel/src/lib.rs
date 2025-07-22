#![no_std]
#![no_main]

mod editor;
mod gdt;
mod heap;
mod idt;
mod isr;
mod keyboard;
mod log;
mod memory;
mod paging;
mod pic;
mod port_io;
mod ramfs;
mod shell;
mod syscalls;
mod tests;
mod timer;
mod vga;

use core::panic::PanicInfo;
use vga::{Terminal, VgaColor};

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    unsafe {
        TERMINAL.set_full_color(VgaColor::Black, VgaColor::Red);
        TERMINAL.clear();
        TERMINAL.write_line(":(");
        TERMINAL.write_line("========== KERNEL PANIC ==========");
        if let Some(msg) = info.payload().downcast_ref::<&str>() {
            use core::fmt::Write;
            let _ = TERMINAL.write_fmt(format_args!("{}\n", msg));
        }
        if let Some(loc) = info.location() {
            use core::fmt::Write;
            let _ = TERMINAL.write_fmt(format_args!("Location: {}:{}\n", loc.file(), loc.line()));
        }
        TERMINAL.write_line("==================================");
    }
    loop {}
}

static mut TERMINAL: Terminal = Terminal::new();

#[no_mangle]
pub extern "C" fn rust_main() {
    unsafe {
        TERMINAL.initialize();
        TERMINAL.write_line("Hello from Rust kernel!");
        success!("Terminal initialized");
        gdt::init_gdt();
        success!("GDT loaded");
        idt::init_idt();
        success!("IDT loaded");
        pic::init_pic();
        timer::init_timer(1000);
        memory::initialize();
        heap::init_heap();
        ramfs::fs_init();
        paging::vmm_init();
        paging::vmm_enable();
        keyboard::keyboard_install();
        tests::run_tests();
        shell::init();
    }
    loop {}
}
