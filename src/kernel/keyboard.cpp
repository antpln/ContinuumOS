#include <stdint.h>
#include "kernel/port_io.h"
#include "kernel/pic.h"
#include "kernel/isr.h"
#include "kernel/keyboard.h"
#include "stdio.h"
#include "kernel/shell.h"
#include "kernel/debug.h"
#include "kernel/syscalls.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"

static bool shift_pressed = false;
static bool caps_lock_active = false;

static uint8_t scancode_to_ascii[128] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=', 0,  0,
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0,  '*', 0,  ' ',
    // Remaining entries are unused
};

char toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

char kb_to_ascii(keyboard_event event) {
    if (event.scancode >= 128) return 0;

    char ascii = scancode_to_ascii[event.scancode];
    if (event.shift || event.caps_lock) {
        if (ascii >= 'a' && ascii <= 'z') ascii = toupper(ascii);
    }
    return ascii;
}

keyboard_event read_keyboard() {
    static bool extended = false;
    uint8_t scancode = inb(KBD_DATA_PORT);

    keyboard_event event = {
        .scancode   = scancode,
        .shift      = shift_pressed,
        .caps_lock  = caps_lock_active,
        .ctrl       = false,
        .alt        = false,
        .special    = false,
        .release    = false,
        .enter      = false,
        .backspace  = false,
        .up_arrow   = false,
        .down_arrow = false,
        .left_arrow = false,
        .right_arrow= false,
    };

    if (scancode == 0xE0) {
        extended = true;
        event.special = true;
        return event;
    }

    if (extended) {
        switch (scancode) {
            case 0x48: event.up_arrow    = true; break;
            case 0x50: event.down_arrow  = true; break;
            case 0x4B: event.left_arrow  = true; break;
            case 0x4D: event.right_arrow = true; break;
        }
        event.special = true;
        extended = false;
    }

    if (scancode == KBD_SCANCODE_SHIFT_LEFT || scancode == KBD_SCANCODE_SHIFT_RIGHT) {
        shift_pressed = true;
    } else if (scancode == (KBD_SCANCODE_SHIFT_LEFT | KBD_SCANCODE_RELEASE) ||
               scancode == (KBD_SCANCODE_SHIFT_RIGHT | KBD_SCANCODE_RELEASE)) {
        shift_pressed = false;
    }

    event.shift = shift_pressed;
    event.release = scancode & KBD_SCANCODE_RELEASE;
    if (scancode == KBD_SCANCODE_CAPS_LOCK) {
        caps_lock_active = !caps_lock_active;
    }
    event.caps_lock = caps_lock_active;
    event.enter = scancode == KBD_SCANCODE_ENTER;
    event.backspace = scancode == KBD_SCANCODE_BACKSPACE;

    return event;
}

void keyboard_callback(registers_t *regs) {
    (void)regs; // Unused
    keyboard_event event = read_keyboard();
    char c = kb_to_ascii(event);
    if (c) {
        keyboard_buffer_push(c);
    }
    Process* proc = scheduler_current_process();
    if (proc) {
        IOEvent io_event;
        io_event.type = EVENT_KEYBOARD;
        io_event.data.keyboard = event;
        push_io_event(proc, io_event);
    }
    shell_handle_key(event);
}

void wait_for_input_clear() {
    while (inb(0x64) & 2) { /* wait */ }
}

void keyboard_flush() {
    while (inb(0x64) & 1) {
        inb(0x60); // Discard
    }
}

void keyboard_reset() {
    wait_for_input_clear();
    outb(0x64, 0xFF); // Reset command
    (void)inb(0x60);  // Read ACK or self-test
}

void keyboard_enable() {
    keyboard_reset();
    keyboard_flush();

    wait_for_input_clear();
    outb(0x64, 0xAE); // Enable keyboard interface
    wait_for_input_clear();
    outb(0x60, 0xF4); // Enable scanning

    for (int i = 0; i < 1000; ++i) {
        if (inb(0x64) & 1) {
            uint8_t response = inb(0x60);
            debug("[KB] Keyboard response: 0x%x", response);
            if (response == 0xFA) return;
        }
    }
    error("[KB] Warning: No ACK received from keyboard");
}

void keyboard_install() {
    debug("[KB] Enabling keyboard...");
    keyboard_enable();
    register_interrupt_handler(33, keyboard_callback);
    pic_unmask_irq(1);
}

void keyboard_check_status() {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        debug("[KB] Output buffer has data!");
    }
}

void keyboard_poll() {
    while (1) {
        if (inb(0x64) & 0x01) {
            read_keyboard();
        }
    }
}
