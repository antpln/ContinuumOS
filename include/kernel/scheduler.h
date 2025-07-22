#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "kernel/process.h"

#define MAX_PROCESSES 32

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

#endif // SCHEDULER_H
