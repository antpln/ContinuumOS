use crate::vga::Terminal;
use crate::keyboard::KeyboardEvent;

extern "C" {
    static mut TERMINAL: Terminal;
}

static mut ACTIVE: bool = false;

pub fn is_active() -> bool { unsafe { ACTIVE } }

pub fn start() {
    unsafe {
        ACTIVE = true;
        TERMINAL.write_line("[EDITOR] not implemented");
    }
}

pub fn handle_key(_event: KeyboardEvent) {
    // placeholder: just exit on Enter
    unsafe {
        ACTIVE = false;
    }
}
