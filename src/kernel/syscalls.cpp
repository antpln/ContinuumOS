#include "kernel/ramfs.h"
#include "kernel/syscalls.h"
#include "kernel/keyboard.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/gui.h"
#include "kernel/vga.h"
#include "kernel/terminal_windows.h"
#include "kernel/framebuffer.h"
#include "kernel/serial.h"
#include "kernel/pci.h"
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
        // ...other syscalls...
        default:
            // Unknown syscall
            break;
    }
}
