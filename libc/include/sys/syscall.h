#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H
#include <stdint.h>
#include <stddef.h>
#include <sys/events.h>
#include <sys/gui.h>
#include <kernel/vfs.h>

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
#define SYSCALL_CONSOLE_WRITE 0x87
#define SYSCALL_PCI_REGISTER_LISTENER 0x88
#define SYSCALL_PCI_UNREGISTER_LISTENER 0x89
#define SYSCALL_ALLOC 0x8A
#define SYSCALL_FREE 0x8B
#define SYSCALL_REALLOC 0x8C
#define SYSCALL_VFS_OPEN 0x8D
#define SYSCALL_VFS_READ 0x8E
#define SYSCALL_VFS_WRITE 0x8F
#define SYSCALL_VFS_CLOSE 0x90
#define SYSCALL_VFS_SEEK 0x91
#define SYSCALL_VFS_CREATE 0x92
#define SYSCALL_VFS_REMOVE 0x93
#define SYSCALL_VFS_STAT 0x94
#define SYSCALL_VFS_MKDIR 0x95
#define SYSCALL_VFS_RMDIR 0x96
#define SYSCALL_VFS_READDIR 0x97
#define SYSCALL_VFS_NORMALIZE_PATH 0x98
#define SYSCALL_GRAPHICS_ENSURE_WINDOW 0x99
#define SYSCALL_GRAPHICS_PUT_CHAR 0x9A
#define SYSCALL_GRAPHICS_PRESENT 0x9B
#define SYSCALL_GRAPHICS_SET_CURSOR 0x9C
#define SYSCALL_GRAPHICS_GET_CURSOR 0x9D
#define SYSCALL_GRAPHICS_COLUMNS 0x9E
#define SYSCALL_GRAPHICS_ROWS 0x9F
#define SYSCALL_FRAMEBUFFER_AVAILABLE 0xA0
#define SYSCALL_SCHED_GETPID 0xA1
#define SYSCALL_SCHED_SET_FOREGROUND 0xA2
#define SYSCALL_SCHED_GET_FOREGROUND 0xA3
#define SYSCALL_TERMINAL_MAKE_COLOR 0xA4
#define SYSCALL_TERMINAL_PUT_AT 0xA5
#define SYSCALL_TERMINAL_SET_CURSOR 0xA6

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

static inline void syscall_console_write(const char* buffer, size_t size) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_CONSOLE_WRITE), "b"(buffer), "c"(size)
        : "memory"
    );
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

static inline void syscall_pci_register_listener(uint16_t vendor_id, uint16_t device_id) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_PCI_REGISTER_LISTENER), "b"(vendor_id), "c"(device_id)
        : "memory"
    );
}

static inline void syscall_pci_unregister_listener() {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_PCI_UNREGISTER_LISTENER)
        : "memory"
    );
}

static inline void* syscall_alloc(size_t size) {
    void* ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_ALLOC), "b"(size)
        : "memory"
    );
    return ret;
}

static inline void syscall_free(void* ptr) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_FREE), "b"(ptr)
        : "memory"
    );
}

static inline void* syscall_realloc(void* ptr, size_t size) {
    void* ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_REALLOC), "b"(ptr), "c"(size)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_open(const char* path, vfs_file_t* file) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_OPEN), "b"(path), "c"(file)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_READ), "b"(file), "c"(buffer), "d"(size)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_WRITE), "b"(file), "c"(buffer), "d"(size)
        : "memory"
    );
    return ret;
}

static inline void syscall_vfs_close(vfs_file_t* file) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_VFS_CLOSE), "b"(file)
        : "memory"
    );
}

static inline int syscall_vfs_seek(vfs_file_t* file, uint32_t position) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_SEEK), "b"(file), "c"(position)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_create(const char* path) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_CREATE), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_remove(const char* path) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_REMOVE), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_stat(const char* path, vfs_dirent_t* info) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_STAT), "b"(path), "c"(info)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_mkdir(const char* path) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_MKDIR), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_rmdir(const char* path) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_RMDIR), "b"(path)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_readdir(const char* path, vfs_dirent_t* entries, int max_entries) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_READDIR), "b"(path), "c"(entries), "d"(max_entries)
        : "memory"
    );
    return ret;
}

static inline int syscall_vfs_normalize_path(const char* path, char* normalized) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_VFS_NORMALIZE_PATH), "b"(path), "c"(normalized)
        : "memory"
    );
    return ret;
}

static inline void syscall_graphics_ensure_window(void) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_GRAPHICS_ENSURE_WINDOW)
        : "memory"
    );
}

static inline void syscall_graphics_put_char(size_t column, size_t row, char ch, uint8_t color) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_GRAPHICS_PUT_CHAR), "b"(column), "c"(row), "d"(ch), "S"(color)
        : "memory"
    );
}

static inline void syscall_graphics_present(void) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_GRAPHICS_PRESENT)
        : "memory"
    );
}

static inline void syscall_graphics_set_cursor(size_t row, size_t column, int active) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_GRAPHICS_SET_CURSOR), "b"(row), "c"(column), "d"(active)
        : "memory"
    );
}

static inline int syscall_graphics_get_cursor(size_t* row, size_t* column) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GRAPHICS_GET_CURSOR), "b"(row), "c"(column)
        : "memory"
    );
    return ret;
}

static inline size_t syscall_graphics_columns(void) {
    size_t ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GRAPHICS_COLUMNS)
        : "memory"
    );
    return ret;
}

static inline size_t syscall_graphics_rows(void) {
    size_t ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GRAPHICS_ROWS)
        : "memory"
    );
    return ret;
}

static inline int syscall_framebuffer_is_available(void) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FRAMEBUFFER_AVAILABLE)
        : "memory"
    );
    return ret;
}

static inline int syscall_scheduler_getpid(void) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SCHED_GETPID)
        : "memory"
    );
    return ret;
}

static inline int syscall_scheduler_set_foreground(int pid) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SCHED_SET_FOREGROUND), "b"(pid)
        : "memory"
    );
    return ret;
}

static inline int syscall_scheduler_get_foreground(void) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SCHED_GET_FOREGROUND)
        : "memory"
    );
    return ret;
}

static inline uint8_t syscall_terminal_make_color(uint32_t foreground, uint32_t background) {
    uint32_t ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TERMINAL_MAKE_COLOR), "b"(foreground), "c"(background)
        : "memory"
    );
    return (uint8_t)ret;
}

static inline void syscall_terminal_put_at(char ch, uint8_t color, size_t column, size_t row) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_TERMINAL_PUT_AT), "b"(ch), "c"(color), "d"(column), "S"(row)
        : "memory"
    );
}

static inline void syscall_terminal_set_cursor(size_t row, size_t column) {
    asm volatile (
        "int $0x80"
        :
        : "a"(SYSCALL_TERMINAL_SET_CURSOR), "b"(row), "c"(column)
        : "memory"
    );
}

#ifdef __cplusplus
}
#endif

#endif // LIBC_SYSCALL_H
