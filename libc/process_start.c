#include "process.h"
#include <sys/syscall.h>

int start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size) {
    return syscall_start_process(name, entry, speculative, stack_size);
}
