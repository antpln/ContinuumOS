#include "kernel/isr.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"
#include "kernel/terminal_windows.h"
#include "kernel/vga.h"
#include "kernel/debug.h"

extern Terminal terminal;

#define SCHEDULER_QUANTUM_TICKS 10 // Number of timer ticks per quantum

Process* process_table[MAX_PROCESSES];
int process_count = 0;
int current_process_idx = -1;

static uint32_t xorshift32_state = 2463534242; // Arbitrary nonzero seed
static int quantum_counter = 0;

static registers_t* last_regs = NULL;

// Trampoline to complete a context switch after returning from an interrupt/syscall
extern "C" void switch_to_trampoline();
// Next context to switch to (used by the trampoline)
extern "C" {
CPUContext* g_next_context = nullptr;
}

static Process* foreground_proc = nullptr;
static Process* foreground_stack[MAX_PROCESSES];
static int foreground_stack_top = -1;

void scheduler_init() {
    process_count = 0;
    current_process_idx = -1;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_table[i] = NULL;
    }
    foreground_proc = nullptr;
    foreground_stack_top = -1;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        foreground_stack[i] = nullptr;
    }
}

int scheduler_add_process(Process* proc) {
    if (process_count >= MAX_PROCESSES) return -1;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (process_table[i] == NULL) {
            process_table[i] = proc;
            process_count++;
            if (current_process_idx == -1) current_process_idx = i;
            if (proc->tickets <= 0) proc->tickets = 1; // Default to 1 ticket
            return 0;
        }
    }
    return -1;
}

int scheduler_remove_process(int pid) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (process_table[i] && process_table[i]->pid == pid) {
            process_table[i] = NULL;
            process_count--;
            if (current_process_idx == i) {
                // Advance to next process
                scheduler_next_process();
            }
            return 0;
        }
    }
    return -1;
}

uint32_t xorshift32() {
    xorshift32_state ^= xorshift32_state << 13;
    xorshift32_state ^= xorshift32_state >> 17;
    xorshift32_state ^= xorshift32_state << 5;
    return xorshift32_state;
}

Process* scheduler_next_process() {
    if (process_count == 0) return NULL;
    int total_tickets = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && proc->alive && proc->hook_count == 0) {
            // Process is alive and has no hooks (is runnable)
            total_tickets += proc->tickets;
        }
    }
    if (total_tickets == 0) {
        for (int i = 0; i < MAX_PROCESSES; ++i) {
            Process* proc = process_table[i];
        }
        return NULL;
    }
    int winner = xorshift32() % total_tickets;
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && proc->alive && proc->hook_count == 0) {
            count += proc->tickets;
            if (winner < count) {
                current_process_idx = i;
                return proc;
            }
        }
    }
    return NULL;
}

// Returns 1 if process is eligible to run (no hooks, or at least one triggered hook), 0 otherwise
int process_is_eligible(Process* proc, HookType event_type, uint64_t event_value) {
    if (!proc || !proc->alive) return 0;
    if (proc->hook_count == 0) return 1; // No hooks, always eligible
    for (int i = 0; i < proc->hook_count; ++i) {
        if (proc->hooks[i].type == event_type && proc->hooks[i].trigger_value == event_value) {
            return 1;
        }
    }
    return 0;
}

// Lottery scheduler: consider processes with no hooks, or with triggered hooks
Process* scheduler_next_eligible_process(HookType event_type, uint64_t event_value) {
    if (process_count == 0) return NULL;
    int total_tickets = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (process_table[i] && process_table[i]->alive && process_is_eligible(process_table[i], event_type, event_value)) {
            total_tickets += process_table[i]->tickets;
        }
    }
    if (total_tickets == 0) return NULL;
    int winner = xorshift32() % total_tickets;
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (process_table[i] && process_table[i]->alive && process_is_eligible(process_table[i], event_type, event_value)) {
            count += process_table[i]->tickets;
            if (winner < count) {
                current_process_idx = i;
                return process_table[i];
            }
        }
    }
    return NULL;
}

Process* scheduler_current_process() {
    if (current_process_idx < 0 || current_process_idx >= MAX_PROCESSES) return NULL;
    return process_table[current_process_idx];
}

void set_process_tickets(Process* proc, int tickets) {
    if (proc && tickets > 0) {
        proc->tickets = tickets;
    }
}

// Called by process to yield and wait for an event
void process_yield_for_event(Process* proc, HookType event_type, uint64_t event_value) {
    if (!proc) return;
    if (!process_has_matching_hook(proc, event_type, event_value)) {
        process_register_hook(proc, event_type, event_value);
    }
    // Note: We don't set alive = 0 here anymore.
    // The process is still alive, just waiting for an event.
    // The scheduler will check hooks to determine if it's runnable.
}

// Called by event source to resume processes waiting for an event
void scheduler_resume_processes_for_event(HookType event_type, uint64_t event_value) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && process_has_matching_hook(proc, event_type, event_value)) {
            // Process is already alive, just remove the hooks so it becomes runnable
            int removed = 0;
            while (process_remove_hook(proc, event_type, event_value) == 0) {
                // Remove all matching hooks in case multiple were registered
                ++removed;
            }
        }
    }
}

static void dispatch_focus_event(Process* proc, int code, int value) {
    if (!proc || !proc->alive) return;
    IOEvent event;
    event.type = EVENT_PROCESS;
    event.data.process.code = code;
    event.data.process.value = value;
    push_io_event(proc, event);
    scheduler_resume_processes_for_event(HookType::SIGNAL, (uint64_t)proc->pid);
}

static void scheduler_switch_foreground(Process* prev, Process* next) {
    if (prev == next) {
        if (next) {
            terminal_windows::activate_process(next, terminal);
        }
        return;
    }
    foreground_proc = next;
    if (prev && prev != next) {
        dispatch_focus_event(prev, PROCESS_EVENT_FOCUS_LOST, next ? next->pid : -1);
    }
    if (next && prev != next) {
        dispatch_focus_event(next, PROCESS_EVENT_FOCUS_GAINED, next->pid);
    }
    terminal_windows::activate_process(next, terminal);
}

void context_switch(registers_t* regs) {
    Process* current = scheduler_current_process();
    if (!current) return;
    
    // If current process is alive, save its state
    if (current->alive) {
        current->current_state.context.eip = regs->eip;
        current->current_state.context.esp = regs->esp;
        current->current_state.context.ebp = regs->ebp;
        current->current_state.context.eax = regs->eax;
        current->current_state.context.ebx = regs->ebx;
        current->current_state.context.ecx = regs->ecx;
        current->current_state.context.edx = regs->edx;
        current->current_state.context.esi = regs->esi;
        current->current_state.context.edi = regs->edi;
        current->current_state.context.eflags = regs->eflags;
    }

    // Select next process
    Process* next = scheduler_next_process();
    
    // If no valid next process and current is dead, we have a problem
    if (!next) {
        if (!current->alive) {
            PANIC("No runnable processes left after current process exit");
        }
        return;
    }
    
    // Don't "switch" to the same process unless current is dead
    if (next == current && current->alive) {
        return;
    }

    // Program a trampoline return into the next process
    g_next_context = &next->current_state.context;
    regs->eip = (uint32_t)switch_to_trampoline;

    last_regs = regs;
}

void scheduler_on_tick(registers_t* regs) {
    last_regs = regs;
    quantum_counter++;
    if (quantum_counter >= SCHEDULER_QUANTUM_TICKS) {
        quantum_counter = 0;
        context_switch(regs);
    }
}

void scheduler_force_switch() {
    if (last_regs)
        context_switch(last_regs);
}

void scheduler_force_switch_with_regs(registers_t* regs) {
    if (regs)
        context_switch(regs);
}

// Declare the trampoline as an external assembly function
extern "C" void switch_to_trampoline();

void scheduler_exit_current_and_switch(registers_t* regs) {
    (void)regs; // Mark unused parameter
    
    // Current process is already dead (killed before calling this)
    // Select the next alive process, ignoring hooks
    // We can't use scheduler_next_process() because it filters out hooked processes
    // When exiting, we need to switch to ANY alive process
    
    Process* next = NULL;
    int total_tickets = 0;
    
    // Calculate total tickets for all alive processes (ignore hooks)
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && proc->alive) {
            total_tickets += proc->tickets;
        }
    }
    
    if (total_tickets == 0) {
        PANIC("No runnable processes left after process exit");
    }
    
    // Lottery selection from all alive processes
    int winner = xorshift32() % total_tickets;
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && proc->alive) {
            count += proc->tickets;
            if (winner < count) {
                next = proc;
                current_process_idx = i;
                break;
            }
        }
    }
    
    if (!next) {
        PANIC("Failed to select next process after exit");
    }
        
    // Set up the next context
    // Use volatile to prevent compiler optimization issues
    volatile CPUContext** ctx_ptr = (volatile CPUContext**)&g_next_context;
    *ctx_ptr = &next->current_state.context;
    
    // Memory barrier to ensure g_next_context is written before jumping
    asm volatile("" ::: "memory");
    
    // Get the address of the trampoline and jump to it
    // We must use jmp, not call, so no return address is pushed
    // Using a function pointer cast ensures proper linkage in all build configurations
    void (*trampoline_ptr)() = switch_to_trampoline;
    
    asm volatile(
        "jmp *%0"
        :
        : "r"(trampoline_ptr)
        : "memory"
    );
    
    // This point should never be reached
    __builtin_unreachable();
}

void scheduler_start() {
    Process* proc = scheduler_current_process();
    if (!proc) return;
    asm volatile(
        "mov %0, %%esp\n"
        "mov %1, %%ebp\n"
        "jmp *%2\n"
        :
        : "r"(proc->current_state.context.esp),
          "r"(proc->current_state.context.ebp),
          "r"(proc->current_state.context.eip)
        : "memory"
    );
}

void scheduler_set_foreground(Process* proc) {
    if (foreground_proc == proc) return;
    Process* previous = foreground_proc;
    if (previous && previous != proc) {
        if (foreground_stack_top + 1 < MAX_PROCESSES) {
            foreground_stack[++foreground_stack_top] = previous;
        }
    }
    scheduler_switch_foreground(previous, proc);
}

Process* scheduler_get_foreground() {
    return foreground_proc;
}

void scheduler_restore_foreground(Process* owner) {
    if (owner && owner != foreground_proc) return;
    Process* previous = foreground_proc;
    Process* target = nullptr;
    if (foreground_stack_top >= 0) {
        target = foreground_stack[foreground_stack_top--];
    }
    // Clear all hooks from the restored process so it becomes immediately runnable
    if (target && target->alive) {
        target->hook_count = 0;
    }
    scheduler_switch_foreground(previous, target);
}
