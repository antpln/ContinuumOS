use crate::port_io::outb;
use core::ptr::write_volatile;

#[repr(u8)]
#[derive(Copy, Clone)]
pub enum VgaColor {
    Black = 0,
    Blue = 1,
    Green = 2,
    Cyan = 3,
    Red = 4,
    Magenta = 5,
    Brown = 6,
    LightGrey = 7,
    DarkGrey = 8,
    LightBlue = 9,
    LightGreen = 10,
    LightCyan = 11,
    LightRed = 12,
    LightMagenta = 13,
    LightBrown = 14,
    White = 15,
}

const VGA_WIDTH: usize = 80;
const VGA_HEIGHT: usize = 25;

pub struct Terminal {
    row: usize,
    column: usize,
    color: u8,
    buffer: *mut u16,
}

impl Terminal {
    pub const fn new() -> Self {
        Self {
            row: 0,
            column: 0,
            color: 0,
            buffer: 0xb8000 as *mut u16,
        }
    }

    pub fn initialize(&mut self) {
        self.row = 0;
        self.column = 0;
        self.color = Self::make_color(VgaColor::LightGrey, VgaColor::Black);
        for y in 0..VGA_HEIGHT {
            for x in 0..VGA_WIDTH {
                unsafe {
                    write_volatile(
                        self.buffer.add(y * VGA_WIDTH + x),
                        Self::make_entry(b' ' as u8, self.color),
                    );
                }
            }
        }
    }

    fn make_entry(uc: u8, color: u8) -> u16 {
        (uc as u16) | ((color as u16) << 8)
    }

    pub fn make_color(fg: VgaColor, bg: VgaColor) -> u8 {
        (fg as u8) | ((bg as u8) << 4)
    }

    fn putentry_at(&mut self, c: u8, color: u8, x: usize, y: usize) {
        unsafe {
            write_volatile(
                self.buffer.add(y * VGA_WIDTH + x),
                Self::make_entry(c, color),
            );
        }
    }

    pub fn putchar(&mut self, c: u8) {
        if c == b'\n' {
            self.new_line();
            return;
        }
        self.putentry_at(c, self.color, self.column, self.row);
        self.column += 1;
        if self.column == VGA_WIDTH {
            self.new_line();
        }
        self.update_cursor();
    }

    pub fn writestring(&mut self, s: &str) {
        for byte in s.bytes() {
            self.putchar(byte);
        }
    }

    pub fn write_line(&mut self, s: &str) {
        self.writestring(s);
        self.new_line();
    }

    fn new_line(&mut self) {
        self.column = 0;
        self.row += 1;
        if self.row == VGA_HEIGHT {
            self.scroll();
            self.row = VGA_HEIGHT - 1;
        }
        self.update_cursor();
    }

    fn scroll(&mut self) {
        for y in 0..(VGA_HEIGHT - 1) {
            for x in 0..VGA_WIDTH {
                let next = unsafe { *self.buffer.add((y + 1) * VGA_WIDTH + x) };
                unsafe {
                    write_volatile(self.buffer.add(y * VGA_WIDTH + x), next);
                }
            }
        }
        for x in 0..VGA_WIDTH {
            unsafe {
                write_volatile(
                    self.buffer.add((VGA_HEIGHT - 1) * VGA_WIDTH + x),
                    Self::make_entry(b' ' as u8, self.color),
                );
            }
        }
    }

    fn update_cursor(&self) {
        let pos = self.row * VGA_WIDTH + self.column;
        unsafe {
            outb(0x3D4, 0x0F);
            outb(0x3D5, (pos & 0xFF) as u8);
            outb(0x3D4, 0x0E);
            outb(0x3D5, ((pos >> 8) & 0xFF) as u8);
        }
    }

    pub fn set_color(&mut self, color: u8) {
        self.color = color;
    }

    pub fn set_full_color(&mut self, fg: VgaColor, bg: VgaColor) {
        self.color = Self::make_color(fg, bg);
    }

    pub fn put_at(&mut self, c: u8, color: u8, x: usize, y: usize) {
        self.putentry_at(c, color, x, y);
    }

    pub fn set_cursor(&mut self, row: usize, column: usize) {
        self.row = row;
        self.column = column;
        self.update_cursor();
    }

    pub fn get_vga_height(&self) -> usize {
        VGA_HEIGHT
    }

    pub fn get_vga_width(&self) -> usize {
        VGA_WIDTH
    }

    pub fn clear(&mut self) {
        for y in 0..VGA_HEIGHT {
            for x in 0..VGA_WIDTH {
                unsafe {
                    write_volatile(
                        self.buffer.add(y * VGA_WIDTH + x),
                        Self::make_entry(b' ' as u8, self.color),
                    );
                }
            }
        }
        self.row = 0;
        self.column = 0;
        self.update_cursor();
    }
}

use core::fmt;

impl fmt::Write for Terminal {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.writestring(s);
        Ok(())
    }
}
