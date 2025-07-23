#ifndef LIBC_PROCESS_H
#define LIBC_PROCESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Yield execution to the scheduler
void yield();
// Yield and wait for a specific event (hook)
void yield_for_event(int hook_type, uint64_t trigger_value);
// Start a new process
int start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size);

#ifdef __cplusplus
}
#endif

#endif // LIBC_PROCESS_H
