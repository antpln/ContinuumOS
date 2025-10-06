#ifndef LIBC_SYS_EVENTS_H
#define LIBC_SYS_EVENTS_H

#include <stdint.h>
#include <kernel/keyboard.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEYBOARD,
    EVENT_PROCESS,
} EventType;

typedef struct {
    int code;
    int value;
} process_event_data;

typedef struct {
    EventType type;
    union {
        keyboard_event keyboard;
        process_event_data process;
    } data;
} IOEvent;

#define PROCESS_EVENT_FOCUS_GAINED 1
#define PROCESS_EVENT_FOCUS_LOST   2

#ifdef __cplusplus
}
#endif

#endif // LIBC_SYS_EVENTS_H
