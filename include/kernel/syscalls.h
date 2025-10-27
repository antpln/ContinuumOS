#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/vfs.h>
#include <kernel/graphics.h>
#include <kernel/vga.h>

int sys_open(const char* path);
int sys_read(int fd, uint8_t* buffer, size_t size);
int sys_write(int fd, const uint8_t* buffer, size_t size);
size_t sys_console_write(const char* buffer, size_t size);
void sys_close(int fd);
char sys_getchar();

void* sys_alloc(size_t size);
void sys_free(void* ptr);
void* sys_realloc(void* ptr, size_t size);

int sys_vfs_open(const char* path, vfs_file_t* file);
int sys_vfs_read(vfs_file_t* file, void* buffer, size_t size);
int sys_vfs_write(vfs_file_t* file, const void* buffer, size_t size);
int sys_vfs_seek(vfs_file_t* file, uint32_t position);
void sys_vfs_close(vfs_file_t* file);
int sys_vfs_create(const char* path);
int sys_vfs_remove(const char* path);
int sys_vfs_stat(const char* path, vfs_dirent_t* info);
int sys_vfs_mkdir(const char* path);
int sys_vfs_rmdir(const char* path);
int sys_vfs_readdir(const char* path, vfs_dirent_t* entries, int max_entries);
int sys_vfs_normalize_path(const char* path, char* normalized_path);

void sys_graphics_ensure_window(void);
void sys_graphics_put_char(size_t column, size_t row, char ch, uint8_t color);
void sys_graphics_present(void);
void sys_graphics_set_cursor(size_t row, size_t column, bool active);
bool sys_graphics_get_cursor(size_t* row, size_t* column);
size_t sys_graphics_columns(void);
size_t sys_graphics_rows(void);
bool sys_framebuffer_is_available(void);

int sys_scheduler_getpid(void);
int sys_scheduler_set_foreground(int pid);
int sys_scheduler_get_foreground(void);

uint8_t sys_terminal_make_color(vga_color foreground, vga_color background);
void sys_terminal_put_at(char ch, uint8_t color, size_t column, size_t row);
void sys_terminal_set_cursor(size_t row, size_t column);

// Yield execution for current process
void sys_yield();
// Yield and wait for a specific event (hook)
void sys_yield_for_event(int hook_type, uint64_t trigger_value);
// PCI event registration
void sys_pci_register_listener(uint16_t vendor_id, uint16_t device_id);
void sys_pci_unregister_listener();

#include "kernel/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

void syscall_dispatch(registers_t* regs);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_SYSCALLS_H
