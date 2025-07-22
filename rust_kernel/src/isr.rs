use crate::pic;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Registers {
    pub ds: u32,
    pub edi: u32, pub esi: u32, pub ebp: u32, pub esp: u32,
    pub ebx: u32, pub edx: u32, pub ecx: u32, pub eax: u32,
    pub int_no: u32, pub err_code: u32,
    pub eip: u32, pub cs: u32, pub eflags: u32, pub useresp: u32, pub ss: u32,
}

pub type Handler = fn(&mut Registers);

static mut HANDLERS: [Option<Handler>; 256] = [None; 256];

pub fn register_interrupt_handler(n: u8, handler: Handler) {
    unsafe { HANDLERS[n as usize] = Some(handler); }
}

#[no_mangle]
pub extern "C" fn isr_handler(regs: &mut Registers) {
    unsafe {
        if let Some(h) = HANDLERS[regs.int_no as usize] {
            h(regs);
        }

        if regs.int_no >= 32 {
            pic::pic_send_eoi((regs.int_no - 32) as u8);
        }
    }
}
