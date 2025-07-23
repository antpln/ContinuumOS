#include "kernel/isr.h"
#include "kernel/scheduler.h"
#include "kernel/process.h"

#define SCHEDULER_QUANTUM_TICKS 10 // Number of timer ticks per quantum

Process* process_table[MAX_PROCESSES];
int process_count = 0;
int current_process_idx = -1;

static uint32_t xorshift32_state = 2463534242; // Arbitrary nonzero seed
static int quantum_counter = 0;

static registers_t* last_regs = NULL;

void scheduler_init() {
    process_count = 0;
    current_process_idx = -1;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_table[i] = NULL;
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
        if (process_table[i] && process_table[i]->alive) {
            total_tickets += process_table[i]->tickets;
        }
    }
    if (total_tickets == 0) return NULL;
    int winner = xorshift32() % total_tickets;
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (process_table[i] && process_table[i]->alive) {
            count += process_table[i]->tickets;
            if (winner < count) {
                current_process_idx = i;
                return process_table[i];
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
    process_register_hook(proc, event_type, event_value);
    proc->alive = 0; // Mark as not runnable until event is met
}

// Called by event source to resume processes waiting for an event
void scheduler_resume_processes_for_event(HookType event_type, uint64_t event_value) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && !proc->alive && process_has_matching_hook(proc, event_type, event_value)) {
            proc->alive = 1; // Mark as runnable
            process_remove_hook(proc, event_type, event_value);
        }
    }
}

void context_switch(registers_t* regs) {
    Process* current = scheduler_current_process();
    if (!current) return;
    // Save current process state
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

    // Select next process
    Process* next = scheduler_next_process();
    if (!next || next == current) return;

    // Restore next process state
    regs->eip = next->current_state.context.eip;
    regs->esp = next->current_state.context.esp;
    regs->ebp = next->current_state.context.ebp;
    regs->eax = next->current_state.context.eax;
    regs->ebx = next->current_state.context.ebx;
    regs->ecx = next->current_state.context.ecx;
    regs->edx = next->current_state.context.edx;
    regs->esi = next->current_state.context.esi;
    regs->edi = next->current_state.context.edi;
    regs->eflags = next->current_state.context.eflags;

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
