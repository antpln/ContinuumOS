#include "process.h"
#include <sys/syscall.h>

void yield() {
    syscall_yield();
}

void yield_for_event(int hook_type, uint64_t trigger_value) {
    syscall_yield_for_event(hook_type, trigger_value);
}

int process_poll_event(IOEvent* event) {
    return syscall_poll_io_event(event);
}

int process_wait_event(IOEvent* event) {
    return syscall_wait_io_event(event);
}

void process_exit(int status) {
    syscall_exit(status);
    while (1) {
        asm volatile("hlt");
    }
}
