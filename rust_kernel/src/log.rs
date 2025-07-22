use crate::vga::Terminal;
use core::fmt;
use core::fmt::Write;

extern "C" {
    static mut TERMINAL: Terminal;
}

pub fn log(prefix: &str, args: fmt::Arguments) {
    unsafe {
        TERMINAL.writestring(prefix);
        let _ = TERMINAL.write_fmt(args);
        TERMINAL.write_line("");
    }
}

#[macro_export]
macro_rules! debug {
    ($($arg:tt)*) => {
        $crate::log::log("[DEBUG] ", format_args!($($arg)*));
    };
}

#[macro_export]
macro_rules! success {
    ($($arg:tt)*) => {
        $crate::log::log("[SUCCESS] ", format_args!($($arg)*));
    };
}

#[macro_export]
macro_rules! error {
    ($($arg:tt)*) => {
        $crate::log::log("[ERROR] ", format_args!($($arg)*));
    };
}

#[macro_export]
macro_rules! test {
    ($($arg:tt)*) => {
        $crate::log::log("[TEST] ", format_args!($($arg)*));
    };
}
