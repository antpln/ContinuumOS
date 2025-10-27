#ifndef LIBC_SYS_SCHEDULER_H
#define LIBC_SYS_SCHEDULER_H

#include <sys/syscall.h>

static inline int scheduler_getpid(void)
{
    return syscall_scheduler_getpid();
}

static inline int scheduler_set_foreground(int pid)
{
    return syscall_scheduler_set_foreground(pid);
}

static inline int scheduler_get_foreground(void)
{
    return syscall_scheduler_get_foreground();
}

#endif // LIBC_SYS_SCHEDULER_H
