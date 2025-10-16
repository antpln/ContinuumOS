#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H
#include <stdint.h>
#include <sys/events.h>
#include <sys/gui.h>

#ifdef __cplusplus
extern "C" {
#endif

// Syscall numbers
#define SYSCALL_YIELD 0x80
#define SYSCALL_YIELD_FOR_EVENT 0x81
#define SYSCALL_START_PROCESS 0x82
#define SYSCALL_EXIT 0x83
#define SYSCALL_POLL_IO_EVENT 0x84
#define SYSCALL_WAIT_IO_EVENT 0x85
#define SYSCALL_GUI_COMMAND 0x86

static inline void syscall_yield() {
    asm volatile ("int $0x80" : : "a"(SYSCALL_YIELD));
}

static inline void syscall_yield_for_event(int hook_type, uint64_t trigger_value) {
#if defined(__x86_64__)
    register uint64_t rcx asm("rcx") = trigger_value;
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_YIELD_FOR_EVENT), "b"(hook_type), "c"(rcx)
    );
#else
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_YIELD_FOR_EVENT), "b"(hook_type), "c"((uint32_t)trigger_value)
    );
#endif
}

static inline int syscall_start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_START_PROCESS), "b"(name), "c"(entry), "d"(speculative), "S"(stack_size)
    );
    return ret;
}

static inline void syscall_exit(int status) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(status)
        : "memory"
    );
}

static inline int syscall_poll_io_event(IOEvent* event) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_POLL_IO_EVENT), "b"(event)
        : "memory"
    );
    return ret;
}

static inline int syscall_wait_io_event(IOEvent* event) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_WAIT_IO_EVENT), "b"(event)
        : "memory"
    );
    return ret;
}

static inline void syscall_gui_command(const GuiCommand* command) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_GUI_COMMAND), "b"(command)
        : "memory"
    );
}

#ifdef __cplusplus
}
#endif

#endif // LIBC_SYSCALL_H
