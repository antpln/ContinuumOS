#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H
#include <stddef.h>
#include <stdint.h>
#include "kernel/hooks.h"
#include "kernel/keyboard.h"
#include <sys/events.h>

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

// Export the next context symbol for the trampoline
extern struct CPUContext* g_next_context;

typedef struct ProcessState {
    CPUContext context;
    uint32_t* page_directory;
    uint8_t* stack_base;
    uint32_t stack_size;
} ProcessState;

typedef void (*KeyboardHandler)(keyboard_event);

#define PROCESS_MAGIC 0x50524F43u // 'PROC'
#define EVENT_QUEUE_GUARD 0xE17E17E1u
#define MAX_EVENT_QUEUE_SIZE 128
#define MAX_HOOKS_PER_PROCESS 8

typedef struct {
    uint32_t guard_front;
    IOEvent queue[MAX_EVENT_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    uint32_t guard_back;
} EventQueue;

typedef struct Process {
    uint32_t magic;
    int pid;
    const char* name;
    ProcessState current_state;
    int alive;
    int speculative;
    uint64_t logical_time;
    Hook hooks[MAX_HOOKS_PER_PROCESS]; // Array of hooks
    int hook_count;
    EventQueue io_events; // Per-process I/O event queue
    KeyboardHandler keyboard_handler; // Per-process keyboard callback
    int tickets; // Number of tickets for lottery scheduling
} Process;

int create_process(const char* name, void (*entry)(), int speculative);
void kill_process(Process* proc);
void register_keyboard_handler(Process* proc, KeyboardHandler handler);
void set_process_tickets(Process* proc, int tickets); // Set tickets for a process

// IO event queue helpers
void push_io_event(Process* proc, IOEvent event);
int pop_io_event(Process* proc, IOEvent* out_event);
int process_poll_io_event(Process* proc, IOEvent* out_event);
int process_wait_for_io_event(Process* proc, IOEvent* out_event);

// Register a hook for a process
int process_register_hook(Process* proc, HookType type, uint64_t trigger_value);
// Remove a hook from a process
int process_remove_hook(Process* proc, HookType type, uint64_t trigger_value);
// Check if a process has a matching hook
int process_has_matching_hook(Process* proc, HookType type, uint64_t value);

// Start a process (kernel internal helper)
Process* k_start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_PROCESS_H
