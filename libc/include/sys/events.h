#ifndef LIBC_SYS_EVENTS_H
#define LIBC_SYS_EVENTS_H

#include <stdint.h>
#include <kernel/keyboard.h>
#include <kernel/mouse.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEYBOARD,
    EVENT_MOUSE,
    EVENT_PROCESS,
    EVENT_PCI,
} EventType;

typedef struct {
    int code;
    int value;
} process_event_data;

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    int event_type;  // Type of PCI event (device added, removed, etc.)
} pci_event_data;

typedef struct {
    EventType type;
    union {
        keyboard_event keyboard;
        MouseEvent mouse;
        process_event_data process;
        pci_event_data pci;
    } data;
} IOEvent;

#define PROCESS_EVENT_FOCUS_GAINED 1
#define PROCESS_EVENT_FOCUS_LOST   2

// PCI event types
#define PCI_EVENT_DEVICE_ADDED     1
#define PCI_EVENT_DEVICE_REMOVED   2
#define PCI_EVENT_DEVICE_READY     3
#define PCI_EVENT_INTERRUPT        4

#ifdef __cplusplus
}
#endif

#endif // LIBC_SYS_EVENTS_H
