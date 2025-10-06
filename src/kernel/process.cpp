#include "kernel/heap.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"
#include "kernel/debug.h"

// Forward declarations for internal helpers
static inline bool process_is_valid(Process* proc, const char* where);

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

    // Set CPU entry point; stack should be set externally or by allocator
    p.current_state.context.eip = (uint32_t)entry;
    p.current_state.context.esp = 0;
    p.current_state.context.ebp = 0;

    // Return PID to caller; they must insert the process into the scheduler
    return p.pid;
}

void kill_process(Process* proc) {
    if (process_is_valid(proc, "kill")) {
        proc->alive = 0;
    }
}

void register_keyboard_handler(Process* proc, KeyboardHandler handler) {
    if (!process_is_valid(proc, "register_keyboard_handler")) {
        return;
    }
    proc->keyboard_handler = handler;
}

static inline uint32_t irq_save() {
    uint32_t flags;
    asm volatile("pushf\n\tpop %0\n\tcli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    asm volatile("push %0\n\tpopf" :: "r"(flags) : "memory", "cc");
}

static inline int normalize_index(int value) {
    if (value < 0) {
        int mod = (-value) % MAX_EVENT_QUEUE_SIZE;
        return (MAX_EVENT_QUEUE_SIZE - mod) % MAX_EVENT_QUEUE_SIZE;
    }
    return value % MAX_EVENT_QUEUE_SIZE;
}

static inline bool process_is_valid(Process* proc, const char* where) {
    if (!proc) {
        error("[process] null process pointer where=%s", where);
        return false;
    }
    if (proc->magic != PROCESS_MAGIC) {
        error("[process] magic mismatch pid=%d name=%s magic=0x%x where=%s", proc->pid, proc->name ? proc->name : "(null)", proc->magic, where);
        return false;
    }
    return true;
}

static void reset_event_queue(Process* proc, const char* reason) {
    if (!proc) return;
    error("[process] resetting IO queue pid=%d name=%s reason=%s", proc->pid, proc->name ? proc->name : "(null)", reason ? reason : "(unknown)");
    for (int i = 0; i < MAX_EVENT_QUEUE_SIZE; ++i) {
        proc->io_events.queue[i] = IOEvent{};
    }
    proc->io_events.head = 0;
    proc->io_events.tail = 0;
    proc->io_events.count = 0;
    proc->io_events.guard_front = EVENT_QUEUE_GUARD;
    proc->io_events.guard_back = EVENT_QUEUE_GUARD;
}

static bool ensure_event_queue_integrity(Process* proc, const char* where) {
    if (!process_is_valid(proc, where)) {
        return false;
    }

    EventQueue& queue = proc->io_events;
    if (queue.guard_front != EVENT_QUEUE_GUARD || queue.guard_back != EVENT_QUEUE_GUARD) {
        error("[process] queue guard violated pid=%d name=%s front=0x%x back=0x%x where=%s",
              proc->pid, proc->name ? proc->name : "(null)", queue.guard_front, queue.guard_back, where);
        reset_event_queue(proc, "guard-corruption");
        return true; // continue with cleared queue
    }

    int raw_head = queue.head;
    int raw_tail = queue.tail;
    int raw_count = queue.count;
    bool adjusted = false;

    if (raw_head < 0 || raw_head >= MAX_EVENT_QUEUE_SIZE) {
        error("[process] queue head out of range pid=%d name=%s head=%d where=%s", proc->pid, proc->name ? proc->name : "(null)", raw_head, where);
        raw_head = normalize_index(raw_head);
        adjusted = true;
    }

    if (raw_tail < 0 || raw_tail >= MAX_EVENT_QUEUE_SIZE) {
        error("[process] queue tail out of range pid=%d name=%s tail=%d where=%s", proc->pid, proc->name ? proc->name : "(null)", raw_tail, where);
        raw_tail = normalize_index(raw_tail);
        adjusted = true;
    }

    if (raw_count < 0 || raw_count > MAX_EVENT_QUEUE_SIZE) {
        error("[process] queue count out of range pid=%d name=%s count=%d where=%s", proc->pid, proc->name ? proc->name : "(null)", raw_count, where);
        raw_count = 0;
        adjusted = true;
    }

    int distance = raw_head - raw_tail;
    if (distance < 0) distance += MAX_EVENT_QUEUE_SIZE;

    if (raw_count != distance && raw_count != MAX_EVENT_QUEUE_SIZE) {
        error("[process] queue count mismatch pid=%d name=%s count=%d expected=%d where=%s",
              proc->pid, proc->name ? proc->name : "(null)", raw_count, distance, where);
        raw_count = distance;
        adjusted = true;
    }

    if (raw_count == MAX_EVENT_QUEUE_SIZE && distance != 0) {
        error("[process] queue full inconsistency pid=%d name=%s distance=%d where=%s",
              proc->pid, proc->name ? proc->name : "(null)", distance, where);
        raw_count = distance;
        adjusted = true;
    }

    if (adjusted) {
        queue.head = raw_head;
        queue.tail = raw_tail;
        queue.count = raw_count;
    }

    queue.guard_front = EVENT_QUEUE_GUARD;
    queue.guard_back = EVENT_QUEUE_GUARD;
    return true;
}

void push_io_event(Process* proc, IOEvent event) {
    static bool logged_sizes = false;
    if (!logged_sizes) {
        logged_sizes = true;
        //debug("[process] sizeof(IOEvent)=%u sizeof(keyboard_event)=%u queue-bytes=%u", (unsigned)sizeof(IOEvent), (unsigned)sizeof(keyboard_event), (unsigned)(sizeof(IOEvent) * MAX_EVENT_QUEUE_SIZE));
    }
    uint32_t flags = irq_save();
    if (!ensure_event_queue_integrity(proc, "push")) {
        irq_restore(flags);
        return;
    }
    EventQueue& queue = proc->io_events;
    if (queue.count == MAX_EVENT_QUEUE_SIZE) {
        error("[process] event queue full pid=%d name=%s dropping oldest event", proc->pid, proc->name ? proc->name : "(null)");
        queue.tail = (queue.tail + 1) % MAX_EVENT_QUEUE_SIZE;
        if (queue.count > 0) {
            queue.count--;
        }
    }
    queue.queue[queue.head] = event;
    queue.head = (queue.head + 1) % MAX_EVENT_QUEUE_SIZE;
    if (queue.count < MAX_EVENT_QUEUE_SIZE) {
        queue.count++;
    }
    //debug("[process] push event pid=%d type=%d head=%d tail=%d count=%d", proc->pid, event.type, queue.head, queue.tail, queue.count);
    irq_restore(flags);
}

int pop_io_event(Process* proc, IOEvent* out_event) {
    uint32_t flags = irq_save();
    if (!ensure_event_queue_integrity(proc, "pop")) {
        irq_restore(flags);
        return 0;
    }
    EventQueue& queue = proc->io_events;
    if (queue.count == 0) {
        irq_restore(flags);
        return 0; // Empty
    }
    *out_event = queue.queue[queue.tail];
    queue.tail = (queue.tail + 1) % MAX_EVENT_QUEUE_SIZE;
    if (queue.count > 0) {
        queue.count--;
    }
    //debug("[process] pop event pid=%d type=%d head=%d tail=%d count=%d", proc->pid, out_event->type, queue.head, queue.tail, queue.count);
    irq_restore(flags);
    return 1;
}

int process_poll_io_event(Process* proc, IOEvent* out_event) {
    if (!proc || !out_event) return 0;
    return pop_io_event(proc, out_event);
}

int process_wait_for_io_event(Process* proc, IOEvent* out_event) {
    if (!proc || !out_event) return 0;
    if (pop_io_event(proc, out_event)) {
        return 1;
    }
    process_yield_for_event(proc, HookType::SIGNAL, (uint64_t)proc->pid);
    return 0;
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

Process* k_start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size) {
    Process* proc = (Process*)kmalloc(sizeof(Process));
    if (!proc) return NULL;
    // Zero the struct to avoid stale data
    for (size_t i = 0; i < sizeof(Process); ++i) ((uint8_t*)proc)[i] = 0;

    proc->magic = PROCESS_MAGIC;
    proc->pid = create_process(name, entry, speculative);
    proc->name = name;
    proc->speculative = speculative;
    proc->logical_time = 0;
    proc->alive = 1;
    proc->hook_count = 0;
    proc->tickets = 1;
    proc->io_events.guard_front = EVENT_QUEUE_GUARD;
    proc->io_events.guard_back = EVENT_QUEUE_GUARD;
    proc->io_events.head = 0;
    proc->io_events.tail = 0;
    proc->io_events.count = 0;
    for (int i = 0; i < MAX_EVENT_QUEUE_SIZE; ++i) {
        proc->io_events.queue[i] = IOEvent{};
    }
    proc->keyboard_handler = NULL;

    // Allocate and set up stack
    uint32_t stack_top = (uint32_t)kmalloc(stack_size);
    if (!stack_top) return NULL;
    stack_top += stack_size;

    proc->current_state.context.eip = (uint32_t)entry;
    proc->current_state.context.esp = stack_top;
    proc->current_state.context.ebp = stack_top;
    proc->current_state.context.eflags = 0x202; // IF=1, reserved bit set

    proc->current_state.stack_base = (uint8_t*)(stack_top - stack_size);
    proc->current_state.stack_size = stack_size;

    // Insert into scheduler
    scheduler_add_process(proc);
    return proc;
}
