#include "kernel/process.h"
#include "kernel/heap.h"

// Static PID counter
static int next_pid = 1;

int create_process(const char* name, void (*entry)(), int speculative) {
    // This function should allocate and return a new PID
    // Actual process registration in a scheduler is done externally

    Process p;
    p.pid = next_pid++;
    p.name = name;
    p.speculative = speculative;
    p.logical_time = 0;
    p.alive = 1;
    p.wait_hook = 0;
    p.saved_state = 0;

    // Set CPU entry point; stack should be set externally or by allocator
    p.current_state.context.eip = (uint32_t)entry;
    p.current_state.context.esp = 0;
    p.current_state.context.ebp = 0;

    // Return PID to caller; they must insert the process into the scheduler
    return p.pid;
}

void kill_process(Process* proc) {
    if (proc) {
        proc->alive = 0;
    }
}

ProcessState* save_continuation(const Process* p) {
    if (!p) return 0;

    // Allocate new state on kernel heap (replace with actual allocator)
    ProcessState* saved = (ProcessState*)kmalloc(sizeof(ProcessState));
    if (!saved) return 0;

    *saved = p->current_state; // Plain assignment — shallow copy
    return saved;
}

void restore_continuation(Process* p, const ProcessState* state) {
    if (!p || !state) return;

    p->current_state = *state;
}

void register_keyboard_handler(Process* proc, KeyboardHandler handler) {
    proc->keyboard_handler = handler;
}

void push_io_event(Process* proc, IOEvent event) {
    int next_head = (proc->io_events.head + 1) % MAX_EVENT_QUEUE_SIZE;
    if (next_head != proc->io_events.tail) {
        proc->io_events.queue[proc->io_events.head] = event;
        proc->io_events.head = next_head;
    }
}

int pop_io_event(Process* proc, IOEvent* out_event) {
    if (proc->io_events.head == proc->io_events.tail) return 0; // Empty
    *out_event = proc->io_events.queue[proc->io_events.tail];
    proc->io_events.tail = (proc->io_events.tail + 1) % MAX_EVENT_QUEUE_SIZE;
    return 1;
}
