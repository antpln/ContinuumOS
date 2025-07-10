#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "isr.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_SCANCODE_RELEASE 0x80
#define KBD_SCANCODE_SHIFT_LEFT 0x2A
#define KBD_SCANCODE_SHIFT_RIGHT 0x36
#define KBD_SCANCODE_CAPS_LOCK 0x3A
#define KBD_SCANCODE_BACKSPACE 0x0E
#define KBD_SCANCODE_ENTER 0x1C
#define KBD_SCANCODE_CTRL 0x1D
#define KBD_SCANCODE_ALT 0x38

typedef struct keyboard_event {
    uint8_t scancode;
    bool shift;
    bool caps_lock;
    bool ctrl;
    bool alt;
    bool special;
    bool release;
    bool enter;
    bool backspace;
    bool up_arrow;
    bool down_arrow;
    bool left_arrow;
    bool right_arrow;
} keyboard_event;

void keyboard_install();
void keyboard_poll();
char kb_to_ascii(keyboard_event event);

#endif // KEYBOARD_H
