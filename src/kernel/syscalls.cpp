#include "kernel/ramfs.h"
#include "kernel/syscalls.h"
#include "kernel/keyboard.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include <stddef.h>
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 128
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static size_t keyboard_buffer_head = 0;
static size_t keyboard_buffer_tail = 0;

int sys_open(const char* path) {
    FSNode* node = fs_find_by_path(path);
    if (!node) return -1;
    return fs_open(node);
}

int sys_read(int fd, uint8_t* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) return -1;
    return fs_read(fd_table[fd].node, fd_table[fd].offset, size, buffer);
}

int sys_write(int fd, const uint8_t* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].used) return -1;
    return fs_write(fd_table[fd].node, fd_table[fd].offset, size, buffer);
}

void sys_close(int fd) {
    fs_close(fd);
}

// Called from interrupt handler
void keyboard_buffer_push(char c) {
    size_t next_head = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head != keyboard_buffer_tail) {
        keyboard_buffer[keyboard_buffer_head] = c;
        keyboard_buffer_head = next_head;
    }
}

char keyboard_buffer_pop() {
    if (keyboard_buffer_head == keyboard_buffer_tail) return 0; // Buffer empty
    char c = keyboard_buffer[keyboard_buffer_tail];
    keyboard_buffer_tail = (keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

char sys_getchar() {
    // Wait for a key in the buffer
    while (keyboard_buffer_head == keyboard_buffer_tail) {
        // Optionally yield/sleep here for multitasking
    }
    return keyboard_buffer_pop();
}

// Syscall to register keyboard handler for current process
void sys_register_keyboard_handler(KeyboardHandler handler) {
    Process* proc = scheduler_current_process();
    if (proc) {
        register_keyboard_handler(proc, handler);
    }
}

int sys_get_io_event(IOEvent* out_event) {
    Process* proc = scheduler_current_process();
    if (!proc) return 0;
    return pop_io_event(proc, out_event);
}

void sys_yield() {
    Process* proc = scheduler_current_process();
    if (!proc) return;
    // Mark process as not running for this quantum, but still eligible
    // This is a cooperative yield; process remains alive
    // Context switch should be triggered here
    // For now, just return; context switch logic should be handled by the scheduler/interrupt handler
}

void sys_yield_for_event(int hook_type, uint64_t trigger_value) {
    Process* proc = scheduler_current_process();
    if (!proc) return;
    process_yield_for_event(proc, (HookType)hook_type, trigger_value);
    // Context switch should be triggered here
    // For now, just return; context switch logic should be handled by the scheduler/interrupt handler
}

extern "C" void syscall_dispatch(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    switch (syscall_num) {
        case 0x80: // SYSCALL_YIELD
            sys_yield();
            break;
        case 0x81: // SYSCALL_YIELD_FOR_EVENT
            sys_yield_for_event(arg1, arg2);
            break;
        case 0x82: // SYSCALL_START_PROCESS
            // arg1: name, arg2: entry, arg3: speculative, arg4: stack_size
            start_process((const char*)arg1, (void (*)())arg2, (int)arg3, (uint32_t)arg4);
            break;
        // ...other syscalls...
        default:
            // Unknown syscall
            break;
    }
}
