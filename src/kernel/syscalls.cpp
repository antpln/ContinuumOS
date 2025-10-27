#include "kernel/ramfs.h"
#include "kernel/syscalls.h"
#include "kernel/keyboard.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/gui.h"
#include "kernel/vga.h"
#include "kernel/graphics.h"
#include "kernel/terminal_windows.h"
#include "kernel/framebuffer.h"
#include "kernel/serial.h"
#include "kernel/pci.h"
#include "kernel/heap.h"
#include "kernel/vfs.h"
#include <sys/gui.h>
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

extern Terminal terminal;

namespace
{
bool g_console_write_in_progress = false;

class ConsoleWriteGuard
{
  public:
    ConsoleWriteGuard()
    {
        g_console_write_in_progress = true;
    }

    ~ConsoleWriteGuard()
    {
        g_console_write_in_progress = false;
    }
};
} // namespace

size_t sys_console_write(const char* buffer, size_t size)
{
    if (buffer == nullptr || size == 0)
    {
        return 0;
    }

    if (g_console_write_in_progress)
    {
#ifdef DEBUG
        serial_printf("[SYSCON] reentrant write fallback (len=%u)\n", static_cast<unsigned>(size));
#endif
        for (size_t i = 0; i < size; ++i)
        {
            serial_write_char(buffer[i]);
        }
        return size;
    }

    ConsoleWriteGuard guard;

    Process* proc = scheduler_current_process();

    if (!framebuffer::is_available() || proc == nullptr)
    {
        for (size_t i = 0; i < size; ++i)
        {
            terminal.putchar(buffer[i]);
        }
        return size;
    }

    terminal_windows::write_text(terminal, proc, buffer, size);
    return size;
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
        sys_yield();
    }
    return keyboard_buffer_pop();
}

void* sys_alloc(size_t size) {
    return kmalloc(size);
}

void sys_free(void* ptr) {
    if (ptr) {
        kfree(ptr);
    }
}

void* sys_realloc(void* ptr, size_t size) {
    return krealloc(ptr, size);
}

int sys_vfs_open(const char* path, vfs_file_t* file) {
    return vfs_open(path, file);
}

int sys_vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    return vfs_read(file, buffer, size);
}

int sys_vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    return vfs_write(file, buffer, size);
}

int sys_vfs_seek(vfs_file_t* file, uint32_t position) {
    return vfs_seek(file, position);
}

void sys_vfs_close(vfs_file_t* file) {
    vfs_close(file);
}

int sys_vfs_create(const char* path) {
    return vfs_create(path);
}

int sys_vfs_remove(const char* path) {
    return vfs_remove(path);
}

int sys_vfs_stat(const char* path, vfs_dirent_t* info) {
    return vfs_stat(path, info);
}

int sys_vfs_mkdir(const char* path) {
    return vfs_mkdir(path);
}

int sys_vfs_rmdir(const char* path) {
    return vfs_rmdir(path);
}

int sys_vfs_readdir(const char* path, vfs_dirent_t* entries, int max_entries) {
    return vfs_readdir(path, entries, max_entries);
}

int sys_vfs_normalize_path(const char* path, char* normalized_path) {
    return vfs_normalize_path(path, normalized_path);
}

void sys_graphics_ensure_window(void) {
    graphics::ensure_window();
}

void sys_graphics_put_char(size_t column, size_t row, char ch, uint8_t color) {
    graphics::put_char(column, row, ch, color);
}

void sys_graphics_present(void) {
    graphics::present();
}

void sys_graphics_set_cursor(size_t row, size_t column, bool active) {
    graphics::set_cursor(row, column, active);
}

bool sys_graphics_get_cursor(size_t* row, size_t* column) {
    if (!row || !column) {
        return false;
    }
    size_t local_row = 0;
    size_t local_col = 0;
    bool ok = graphics::get_cursor(local_row, local_col);
    if (ok) {
        *row = local_row;
        *column = local_col;
    }
    return ok;
}

size_t sys_graphics_columns(void) {
    return graphics::columns();
}

size_t sys_graphics_rows(void) {
    return graphics::rows();
}

bool sys_framebuffer_is_available(void) {
    return framebuffer::is_available();
}

static Process* find_process_by_pid(int pid) {
    if (pid <= 0) {
        return nullptr;
    }
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        Process* proc = process_table[i];
        if (proc && proc->pid == pid) {
            return proc;
        }
    }
    return nullptr;
}

int sys_scheduler_getpid(void) {
    Process* proc = scheduler_current_process();
    return proc ? proc->pid : -1;
}

int sys_scheduler_set_foreground(int pid) {
    Process* target = find_process_by_pid(pid);
    if (!target) {
        return -1;
    }
    scheduler_set_foreground(target);
    return 0;
}

int sys_scheduler_get_foreground(void) {
    Process* proc = scheduler_get_foreground();
    return proc ? proc->pid : -1;
}

uint8_t sys_terminal_make_color(vga_color foreground, vga_color background) {
    return terminal.make_color(foreground, background);
}

void sys_terminal_put_at(char ch, uint8_t color, size_t column, size_t row) {
    terminal.put_at(ch, color, column, row);
}

void sys_terminal_set_cursor(size_t row, size_t column) {
    terminal.set_cursor(row, column);
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

int sys_wait_io_event(registers_t* regs, IOEvent* out_event) {
    Process* proc = scheduler_current_process();
    if (!proc) return 0;
    if (process_wait_for_io_event(proc, out_event)) {
        return 1;
    }
    scheduler_force_switch_with_regs(regs);
    return 0;
}

void sys_yield_with_regs(registers_t* regs) {
    (void)scheduler_current_process();
    scheduler_force_switch_with_regs(regs);
}

void sys_yield() {
    (void)scheduler_current_process();
    scheduler_force_switch();
}

void sys_yield_for_event_with_regs(registers_t* regs, int hook_type, uint64_t trigger_value) {
    Process* proc = scheduler_current_process();
    if (!proc) return;
    process_yield_for_event(proc, (HookType)hook_type, trigger_value);
    scheduler_force_switch_with_regs(regs);
}

void sys_yield_for_event(int hook_type, uint64_t trigger_value) {
    Process* proc = scheduler_current_process();
    if (!proc) return;
    process_yield_for_event(proc, (HookType)hook_type, trigger_value);
    scheduler_force_switch();
}

static inline void sys_exit_with_regs(registers_t* regs) {
    Process* proc = scheduler_current_process();
    if (proc) {
        // Stop handling input
        register_keyboard_handler(proc, nullptr);
        kill_process(proc);
    }
    // Don't return through the dying process's stack frame
    // Instead, directly switch to the next process
    scheduler_exit_current_and_switch(regs);
}

static void sys_gui_command(const GuiCommand* user_command)
{
    if (user_command == nullptr)
    {
        return;
    }

    GuiCommand command = *user_command;
    Process* proc = scheduler_current_process();
    gui::process_command(command, terminal, proc);
}

void sys_pci_register_listener(uint16_t vendor_id, uint16_t device_id) {
    Process* proc = scheduler_current_process();
    if (proc) {
        pci_register_process_listener(proc, vendor_id, device_id);
    }
}

void sys_pci_unregister_listener() {
    Process* proc = scheduler_current_process();
    if (proc) {
        pci_unregister_process_listener(proc);
    }
}

extern "C" void syscall_dispatch(registers_t* regs) {
    uint32_t syscall_num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    uint32_t arg4 = regs->esi;
    switch (syscall_num) {
        case 0x80: // SYSCALL_YIELD
            sys_yield_with_regs(regs);
            break;
        case 0x81: // SYSCALL_YIELD_FOR_EVENT
            sys_yield_for_event_with_regs(regs, (int)arg1, (uint64_t)arg2);
            break;
        case 0x82: { // SYSCALL_START_PROCESS
            // arg1: name, arg2: entry, arg3: speculative, arg4: stack_size
            Process* p = k_start_process((const char*)arg1, (void (*)())arg2, (int)arg3, (uint32_t)arg4);
            regs->eax = p ? (uint32_t)p->pid : (uint32_t)-1;
            break;
        }
        case 0x83: // SYSCALL_EXIT
            sys_exit_with_regs(regs);
            break;
        case 0x84: // SYSCALL_POLL_IO_EVENT
            regs->eax = sys_get_io_event((IOEvent*)arg1);
            break;
        case 0x85: // SYSCALL_WAIT_IO_EVENT
            regs->eax = sys_wait_io_event(regs, (IOEvent*)arg1);
            break;
        case 0x86: // SYSCALL_GUI_COMMAND
            sys_gui_command((const GuiCommand*)arg1);
            break;
        case 0x87: // SYSCALL_CONSOLE_WRITE
            regs->eax = (uint32_t)sys_console_write((const char*)arg1, (size_t)arg2);
            break;
        case 0x88: // SYSCALL_PCI_REGISTER_LISTENER
            sys_pci_register_listener((uint16_t)arg1, (uint16_t)arg2);
            break;
        case 0x89: // SYSCALL_PCI_UNREGISTER_LISTENER
            sys_pci_unregister_listener();
            break;
        case 0x8A: // SYSCALL_ALLOC
            regs->eax = (uint32_t)sys_alloc((size_t)arg1);
            break;
        case 0x8B: // SYSCALL_FREE
            sys_free((void*)arg1);
            break;
        case 0x8C: // SYSCALL_REALLOC
            regs->eax = (uint32_t)sys_realloc((void*)arg1, (size_t)arg2);
            break;
        case 0x8D: // SYSCALL_VFS_OPEN
            regs->eax = (uint32_t)sys_vfs_open((const char*)arg1, (vfs_file_t*)arg2);
            break;
        case 0x8E: // SYSCALL_VFS_READ
            regs->eax = (uint32_t)sys_vfs_read((vfs_file_t*)arg1, (void*)arg2, (size_t)arg3);
            break;
        case 0x8F: // SYSCALL_VFS_WRITE
            regs->eax = (uint32_t)sys_vfs_write((vfs_file_t*)arg1, (const void*)arg2, (size_t)arg3);
            break;
        case 0x90: // SYSCALL_VFS_CLOSE
            sys_vfs_close((vfs_file_t*)arg1);
            break;
        case 0x91: // SYSCALL_VFS_SEEK
            regs->eax = (uint32_t)sys_vfs_seek((vfs_file_t*)arg1, (uint32_t)arg2);
            break;
        case 0x92: // SYSCALL_VFS_CREATE
            regs->eax = (uint32_t)sys_vfs_create((const char*)arg1);
            break;
        case 0x93: // SYSCALL_VFS_REMOVE
            regs->eax = (uint32_t)sys_vfs_remove((const char*)arg1);
            break;
        case 0x94: // SYSCALL_VFS_STAT
            regs->eax = (uint32_t)sys_vfs_stat((const char*)arg1, (vfs_dirent_t*)arg2);
            break;
        case 0x95: // SYSCALL_VFS_MKDIR
            regs->eax = (uint32_t)sys_vfs_mkdir((const char*)arg1);
            break;
        case 0x96: // SYSCALL_VFS_RMDIR
            regs->eax = (uint32_t)sys_vfs_rmdir((const char*)arg1);
            break;
        case 0x97: // SYSCALL_VFS_READDIR
            regs->eax = (uint32_t)sys_vfs_readdir((const char*)arg1, (vfs_dirent_t*)arg2, (int)arg3);
            break;
        case 0x98: // SYSCALL_VFS_NORMALIZE_PATH
            regs->eax = (uint32_t)sys_vfs_normalize_path((const char*)arg1, (char*)arg2);
            break;
        case 0x99: // SYSCALL_GRAPHICS_ENSURE_WINDOW
            sys_graphics_ensure_window();
            break;
        case 0x9A: // SYSCALL_GRAPHICS_PUT_CHAR
            sys_graphics_put_char((size_t)arg1, (size_t)arg2, (char)arg3, (uint8_t)arg4);
            break;
        case 0x9B: // SYSCALL_GRAPHICS_PRESENT
            sys_graphics_present();
            break;
        case 0x9C: // SYSCALL_GRAPHICS_SET_CURSOR
            sys_graphics_set_cursor((size_t)arg1, (size_t)arg2, arg3 != 0);
            break;
        case 0x9D: // SYSCALL_GRAPHICS_GET_CURSOR
            regs->eax = sys_graphics_get_cursor((size_t*)arg1, (size_t*)arg2);
            break;
        case 0x9E: // SYSCALL_GRAPHICS_COLUMNS
            regs->eax = (uint32_t)sys_graphics_columns();
            break;
        case 0x9F: // SYSCALL_GRAPHICS_ROWS
            regs->eax = (uint32_t)sys_graphics_rows();
            break;
        case 0xA0: // SYSCALL_FRAMEBUFFER_AVAILABLE
            regs->eax = sys_framebuffer_is_available();
            break;
        case 0xA1: // SYSCALL_SCHED_GETPID
            regs->eax = (uint32_t)sys_scheduler_getpid();
            break;
        case 0xA2: // SYSCALL_SCHED_SET_FOREGROUND
            regs->eax = (uint32_t)sys_scheduler_set_foreground((int)arg1);
            break;
        case 0xA3: // SYSCALL_SCHED_GET_FOREGROUND
            regs->eax = (uint32_t)sys_scheduler_get_foreground();
            break;
        case 0xA4: // SYSCALL_TERMINAL_MAKE_COLOR
            regs->eax = (uint32_t)sys_terminal_make_color((vga_color)arg1, (vga_color)arg2);
            break;
        case 0xA5: // SYSCALL_TERMINAL_PUT_AT
            sys_terminal_put_at((char)arg1, (uint8_t)arg2, (size_t)arg3, (size_t)arg4);
            break;
        case 0xA6: // SYSCALL_TERMINAL_SET_CURSOR
            sys_terminal_set_cursor((size_t)arg1, (size_t)arg2);
            break;
        // ...other syscalls...
        default:
            // Unknown syscall
            break;
    }
}
