#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H
#include <stdint.h>
#include <stddef.h>

int sys_open(const char* path);
int sys_read(int fd, uint8_t* buffer, size_t size);
int sys_write(int fd, const uint8_t* buffer, size_t size);
size_t sys_console_write(const char* buffer, size_t size);
void sys_close(int fd);
char sys_getchar();
// Yield execution for current process
void sys_yield();
// Yield and wait for a specific event (hook)
void sys_yield_for_event(int hook_type, uint64_t trigger_value);

#include "kernel/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

void syscall_dispatch(registers_t* regs);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_SYSCALLS_H
