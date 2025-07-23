#include "process.h"
#include <stdint.h>
#include <sys/syscall.h>

// These should be implemented as syscalls in the kernel
extern void syscall_yield();
extern void syscall_yield_for_event(int hook_type, uint64_t trigger_value);

void yield() {
    syscall_yield();
}

void yield_for_event(int hook_type, uint64_t trigger_value) {
    syscall_yield_for_event(hook_type, trigger_value);
}
