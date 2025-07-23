#include "kernel/heap.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"

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

int process_register_hook(Process* proc, HookType type, uint64_t trigger_value) {
    if (!proc || proc->hook_count >= MAX_HOOKS_PER_PROCESS) return -1;
    proc->hooks[proc->hook_count].type = type;
    proc->hooks[proc->hook_count].trigger_value = trigger_value;
    proc->hook_count++;
    return 0;
}

int process_remove_hook(Process* proc, HookType type, uint64_t trigger_value) {
    if (!proc) return -1;
    for (int i = 0; i < proc->hook_count; ++i) {
        if (proc->hooks[i].type == type && proc->hooks[i].trigger_value == trigger_value) {
            // Shift hooks down
            for (int j = i; j < proc->hook_count - 1; ++j) {
                proc->hooks[j] = proc->hooks[j + 1];
            }
            proc->hook_count--;
            return 0;
        }
    }
    return -1;
}

int process_has_matching_hook(Process* proc, HookType type, uint64_t value) {
    if (!proc) return 0;
    for (int i = 0; i < proc->hook_count; ++i) {
        if (proc->hooks[i].type == type && proc->hooks[i].trigger_value == value) {
            return 1;
        }
    }
    return 0;
}

Process* start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size) {
    Process* proc = (Process*)kmalloc(sizeof(Process));
    if (!proc) return NULL;
    proc->pid = create_process(name, entry, speculative);
    proc->name = name;
    proc->speculative = speculative;
    proc->logical_time = 0;
    proc->alive = 1;
    proc->hook_count = 0;
    proc->tickets = 1;
    proc->current_state.context.eip = (uint32_t)entry;
    proc->current_state.context.esp = (uint32_t)kmalloc(stack_size) + stack_size;
    proc->current_state.context.ebp = proc->current_state.context.esp;
    proc->current_state.stack_base = (uint8_t*)(proc->current_state.context.esp - stack_size);
    proc->current_state.stack_size = stack_size;
    // Initialize other fields as needed
    scheduler_add_process(proc);
    return proc;
}
