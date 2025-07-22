use crate::isr::{self, Registers};
use crate::pic;
use crate::port_io::inb;
use crate::editor;

const KBD_DATA_PORT: u16 = 0x60;

#[derive(Default, Clone, Copy)]
pub struct KeyboardEvent {
    pub scancode: u8,
    pub ascii: Option<u8>,
}

fn scancode_to_ascii(scancode: u8) -> Option<u8> {
    match scancode {
        2 => Some(b'1'), 3 => Some(b'2'), 4 => Some(b'3'), 5 => Some(b'4'),
        6 => Some(b'5'), 7 => Some(b'6'), 8 => Some(b'7'), 9 => Some(b'8'),
        10 => Some(b'9'), 11 => Some(b'0'), 12 => Some(b'-'), 13 => Some(b'='),
        14 => Some(0), // backspace
        15 => Some(b'\t'),
        16 => Some(b'q'), 17 => Some(b'w'), 18 => Some(b'e'), 19 => Some(b'r'),
        20 => Some(b't'), 21 => Some(b'y'), 22 => Some(b'u'), 23 => Some(b'i'),
        24 => Some(b'o'), 25 => Some(b'p'),
        28 => Some(b'\n'),
        30 => Some(b'a'), 31 => Some(b's'), 32 => Some(b'd'), 33 => Some(b'f'),
        34 => Some(b'g'), 35 => Some(b'h'), 36 => Some(b'j'), 37 => Some(b'k'),
        38 => Some(b'l'),
        44 => Some(b'z'), 45 => Some(b'x'), 46 => Some(b'c'), 47 => Some(b'v'),
        48 => Some(b'b'), 49 => Some(b'n'), 50 => Some(b'm'),
        57 => Some(b' '),
        _ => None,
    }
}

pub fn keyboard_callback(_regs: &mut Registers) {
    unsafe {
        let scancode = inb(KBD_DATA_PORT);
        let ascii = scancode_to_ascii(scancode);
        let event = KeyboardEvent { scancode, ascii };
        if editor::is_active() {
            editor::handle_key(event);
        } else {
            super::shell::handle_key(event);
        }
        pic::pic_send_eoi(1);
    }
}

pub unsafe fn keyboard_install() {
    isr::register_interrupt_handler(33, keyboard_callback);
    pic::pic_unmask_irq(1);
}
