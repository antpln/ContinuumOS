#include "kernel/scheduler.h"
#include <stddef.h>

Process* process_table[MAX_PROCESSES];
int process_count = 0;
int current_process_idx = -1;

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

Process* scheduler_next_process() {
    if (process_count == 0) return NULL;
    int start = current_process_idx;
    int idx = start;
    do {
        idx = (idx + 1) % MAX_PROCESSES;
        if (process_table[idx] && process_table[idx]->alive) {
            current_process_idx = idx;
            return process_table[idx];
        }
    } while (idx != start);
    return NULL;
}

Process* scheduler_current_process() {
    if (current_process_idx < 0 || current_process_idx >= MAX_PROCESSES) return NULL;
    return process_table[current_process_idx];
}
