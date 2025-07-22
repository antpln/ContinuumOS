use crate::vga::Terminal;
use crate::keyboard::KeyboardEvent;

static WELCOME: &str = "Welcome to ContinuumOS shell";
static PROMPT: &str = "nutshell> ";

static mut BUFFER: [u8; 256] = [0; 256];
static mut INDEX: usize = 0;

extern "C" {
    static mut TERMINAL: Terminal;
}

pub fn init() {
    unsafe {
        TERMINAL.write_line(WELCOME);
        TERMINAL.writestring(PROMPT);
    }
}

pub fn handle_key(event: KeyboardEvent) {
    if let Some(c) = event.ascii {
        unsafe {
            if c == b'\n' {
                BUFFER[INDEX] = 0;
                let line = core::str::from_utf8_unchecked(&BUFFER[..INDEX]);
                TERMINAL.write_line("");
                if line == "help" {
                    TERMINAL.write_line("Available commands: help");
                }
                INDEX = 0;
                TERMINAL.writestring(PROMPT);
            } else {
                if INDEX < BUFFER.len() - 1 {
                    BUFFER[INDEX] = c;
                    INDEX += 1;
                    TERMINAL.putchar(c);
                }
            }
        }
    }
}
