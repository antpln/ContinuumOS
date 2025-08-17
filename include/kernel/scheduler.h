#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "kernel/process.h"

#define MAX_PROCESSES 32

// Forward decl for ISR regs
struct registers;
typedef struct registers registers_t;

// Scheduler process table
extern Process* process_table[MAX_PROCESSES];
extern int process_count;
extern int current_process_idx;

// Add a process to the scheduler
int scheduler_add_process(Process* proc);
// Remove a process from the scheduler
int scheduler_remove_process(int pid);
// Get the next process in round-robin order
Process* scheduler_next_process();
// Get the current process
Process* scheduler_current_process();
// Initialize scheduler
void scheduler_init();
// Returns 1 if process is eligible to run (no hooks, or at least one triggered hook), 0 otherwise
int process_is_eligible(Process* proc, HookType event_type, uint64_t event_value);
// Select next eligible process based on event (lottery among eligible processes)
Process* scheduler_next_eligible_process(HookType event_type, uint64_t event_value);
// Called by process to yield and wait for an event
void process_yield_for_event(Process* proc, HookType event_type, uint64_t event_value);
// Called by event source to resume processes waiting for an event
void scheduler_resume_processes_for_event(HookType event_type, uint64_t event_value);
// Called on each tick for quantum-based scheduling
void scheduler_on_tick(registers_t* regs);
// Force a context switch using last saved registers (from an interrupt)
void scheduler_force_switch();
// Force a context switch using the provided register frame (e.g., from a syscall)
void scheduler_force_switch_with_regs(registers_t* regs);
// Context switch to another process
void context_switch(registers_t* regs);
// Start executing the first scheduled process
void scheduler_start();

// Foreground (keyboard focus) process helpers
void scheduler_set_foreground(Process* proc);
Process* scheduler_get_foreground();

#endif // SCHEDULER_H
