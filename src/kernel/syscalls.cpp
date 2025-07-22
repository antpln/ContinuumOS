#include "kernel/ramfs.h"
#include "kernel/syscalls.h"
#include "kernel/keyboard.h"
#include "kernel/process.h"
#include <stddef.h>

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
