#include <stddef.h>
#include <stdint.h>
#include "kernel/hooks.h"
#include "kernel/keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CPUContext {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
    uint32_t eflags;
} CPUContext;

typedef struct ProcessState {
    CPUContext context;
    uint32_t* page_directory;
    uint8_t* stack_base;
    uint32_t stack_size;
} ProcessState;

typedef void (*KeyboardHandler)(keyboard_event);

#define MAX_EVENT_QUEUE_SIZE 128

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEYBOARD,
    // Future: EVENT_MOUSE, EVENT_TIMER, EVENT_FILE, etc.
} EventType;

typedef struct {
    EventType type;
    union {
        keyboard_event keyboard;
        // Future: mouse_event mouse; file_event file; etc.
    } data;
} IOEvent;

typedef struct {
    IOEvent queue[MAX_EVENT_QUEUE_SIZE];
    int head;
    int tail;
} EventQueue;

typedef struct Process {
    int pid;
    const char* name;
    ProcessState current_state;
    ProcessState* saved_state;
    int alive;
    int speculative;
    uint64_t logical_time;
    Hook* wait_hook;
    EventQueue io_events; // Per-process I/O event queue
    KeyboardHandler keyboard_handler; // Per-process keyboard callback
} Process;

int create_process(const char* name, void (*entry)(), int speculative);
void kill_process(Process* proc);
ProcessState* save_continuation(const Process* p);
void restore_continuation(Process* p, const ProcessState* state);
void register_keyboard_handler(Process* proc, KeyboardHandler handler);

#ifdef __cplusplus
}
#endif
