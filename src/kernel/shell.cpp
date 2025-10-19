#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kernel/keyboard.h>
#include <kernel/isr.h>
#include <kernel/timer.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/vga.h>      
#include <kernel/shell.h>
#include <kernel/pci.h>
#include "editor_process.h"
#include <kernel/process.h>
#include <kernel/blockdev.h>
#include <kernel/fat32.h>
#include <kernel/memory.h>
#include <kernel/scheduler.h>
#include <process.h>
#include <kernel/terminal_windows.h>
#include <kernel/framebuffer.h>

extern Terminal terminal;
extern shell_command_t commands[];

#define SHELL_BUFFER_SIZE    256
#define SHELL_HISTORY_SIZE   16

static Process* g_shell_process = nullptr;

typedef struct {
    char     buffer[SHELL_BUFFER_SIZE];
    size_t   cursor;
    size_t   length;
    bool     input_enabled;
    bool     prompt_visible;
    char     history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
    size_t   history_count;
    int      history_nav;
    size_t   prompt_length;
    size_t   prompt_row;
    size_t   prompt_col;
    size_t   cursor_row;
    size_t   cursor_col;
    size_t   rendered_chars;
    char     prompt_cache[VFS_MAX_PATH + 16];
} ShellState;

static ShellState shell;

// Copy src into dst[n], always null-terminated
static inline void safe_strncpy(char* dst, const char* src, size_t n) {
    strncpy(dst, src, n);
    dst[n - 1] = '\0';
}

// Legacy line clearing helper used when framebuffer GUI is unavailable
static void clear_line(size_t length) {
    for (size_t i = 0; i < length; ++i) printf("\b");
    for (size_t i = 0; i < length; ++i) printf(" ");
    for (size_t i = 0; i < length; ++i) printf("\b");
}

static inline size_t shell_window_width() {
    return Terminal::VGA_WIDTH;
}

static inline size_t shell_window_height() {
    return Terminal::VGA_HEIGHT;
}

static void shell_advance_position(size_t &row, size_t &col) {
    ++col;
    if (col >= shell_window_width()) {
        col = 0;
        if (row + 1 < shell_window_height()) {
            ++row;
        }
    }
}

static void shell_compute_position_from_offset(size_t offset, size_t &row, size_t &col) {
    row = shell.prompt_row;
    col = shell.prompt_col;
    for (size_t i = 0; i < offset; ++i) {
        shell_advance_position(row, col);
    }
}

static void shell_render_input() {
    if (!shell.prompt_visible || g_shell_process == nullptr || !framebuffer::is_available()) {
        return;
    }

    const size_t height = shell_window_height();
    const uint8_t color = terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    shell.buffer[shell.length] = '\0';

    size_t row = shell.prompt_row;
    size_t col = shell.prompt_col;

    auto put_char = [&](char ch) {
        if (row < height) {
            terminal_windows::window_put_char(g_shell_process, col, row, ch, color);
        }
        shell_advance_position(row, col);
    };

    for (size_t i = 0; i < shell.prompt_length; ++i) {
        put_char(shell.prompt_cache[i]);
    }

    for (size_t i = 0; i < shell.length; ++i) {
        put_char(shell.buffer[i]);
    }

    size_t total = shell.prompt_length + shell.length;
    if (shell.rendered_chars > total) {
        size_t diff = shell.rendered_chars - total;
        while (diff-- > 0) {
            put_char(' ');
        }
    }
    shell.rendered_chars = total;

    size_t caret_row;
    size_t caret_col;
    shell_compute_position_from_offset(shell.prompt_length + shell.cursor, caret_row, caret_col);
    shell.cursor_row = caret_row;
    shell.cursor_col = caret_col;
    terminal_windows::window_set_cursor(g_shell_process, caret_row, caret_col, true);
    terminal_windows::window_present(g_shell_process);
}

static void shell_render_input_legacy() {
    size_t previous_total = shell.rendered_chars;
    if (previous_total > 0) {
        clear_line(previous_total);
    }
    printf("%s", shell.prompt_cache);
    printf("%s", shell.buffer);
    size_t tail = (shell.length > shell.cursor) ? (shell.length - shell.cursor) : 0;
    for (size_t i = 0; i < tail; ++i) {
        printf("\b");
    }
    shell.rendered_chars = shell.prompt_length + shell.length;
    shell.cursor_row = 0;
    shell.cursor_col = shell.prompt_length + shell.cursor;
}

// Print the shell prompt
static void shell_print_prompt(void) {
    const char* cwd = vfs_getcwd();
    if (!cwd || !cwd[0]) {
        cwd = "/";
    }
    static const char prefix[] = "nutshell ";
    static const char suffix[] = "> ";
    char prompt[VFS_MAX_PATH + 16];
    size_t pos = 0;

    size_t prefix_len = sizeof(prefix) - 1;
    memcpy(prompt + pos, prefix, prefix_len);
    pos += prefix_len;

    size_t cwd_len = strlen(cwd);
    size_t max_cwd = (sizeof(prompt) - 1) - pos - (sizeof(suffix) - 1);
    if (cwd_len > max_cwd) {
        cwd_len = max_cwd;
    }
    memcpy(prompt + pos, cwd, cwd_len);
    pos += cwd_len;

    size_t suffix_len = sizeof(suffix) - 1;
    memcpy(prompt + pos, suffix, suffix_len);
    pos += suffix_len;

    prompt[pos] = '\0';

    shell.prompt_length = pos;
    memcpy(shell.prompt_cache, prompt, pos);
    shell.prompt_cache[pos] = '\0';

    shell.cursor = 0;
    shell.length = 0;
    shell.buffer[0] = '\0';
    shell.rendered_chars = 0;

    if (g_shell_process != nullptr && framebuffer::is_available()) {
        size_t start_row = 0;
        size_t start_col = 0;
        terminal_windows::window_get_cursor(g_shell_process, start_row, start_col);
        terminal_windows::write_text(terminal, g_shell_process, prompt, pos);
        shell.prompt_row = start_row;
        shell.prompt_col = start_col;
        shell.prompt_visible = true;
        shell_render_input();
    } else {
        printf("%s", prompt);
        shell.prompt_row = 0;
        shell.prompt_col = shell.prompt_length;
        shell.rendered_chars = shell.prompt_length;
        shell.cursor_row = 0;
        shell.cursor_col = shell.prompt_col;
        shell.prompt_visible = true;
    }
}

// Initialize shell state and print welcome message
void shell_init(void) {
    shell.cursor        = 0;
    shell.length        = 0;
    shell.rendered_chars = 0;
    shell.prompt_row    = 0;
    shell.prompt_col    = 0;
    shell.cursor_row    = 0;
    shell.cursor_col    = 0;
    shell.input_enabled = true;
    shell.prompt_visible = false;
    shell.history_count = 0;
    shell.history_nav   = -1;
    shell.prompt_length = 0;
    shell.buffer[0]     = '\0';
    printf("Welcome to nutshell!\n");
    shell_print_prompt();
}

// Add a non-empty command to history
void shell_history_add(const char* cmd) {
    if (!cmd || !*cmd) return;
    size_t idx = shell.history_count % SHELL_HISTORY_SIZE;
    safe_strncpy(shell.history[idx], cmd, SHELL_BUFFER_SIZE);
    shell.history_count++;
    shell.history_nav = -1;
}

// Return the previous command from history, or NULL if none
const char* shell_history_prev(void) {
    if (shell.history_count == 0) return NULL;
    if (shell.history_nav == -1)
        shell.history_nav = (int)shell.history_count - 1;
    else if (shell.history_nav > 0)
        shell.history_nav--;
    return shell.history[shell.history_nav % SHELL_HISTORY_SIZE];
}

// Return the next command from history, or NULL if none
const char* shell_history_next(void) {
    if (shell.history_count == 0) return NULL;
    if (shell.history_nav == -1) return NULL; // Not navigating
    if (shell.history_nav < (int)shell.history_count - 1)
        shell.history_nav++;
    else {
        shell.history_nav = -1;
        return ""; // Clear input if at the end
    }
    return shell.history[shell.history_nav % SHELL_HISTORY_SIZE];
}

// Reset history navigation state
void shell_history_reset(void) {
    shell.history_nav = -1;
}

void shell_set_input_enabled(bool enabled) {
    shell.input_enabled = enabled;
    if (!enabled) {
        shell.prompt_visible = false;
        if (g_shell_process != nullptr && framebuffer::is_available()) {
            terminal_windows::window_set_cursor(g_shell_process, shell.cursor_row, shell.cursor_col, false);
        }
    } else {
        shell.prompt_visible = true;
        if (framebuffer::is_available() && g_shell_process != nullptr) {
            shell_render_input();
        } else {
            shell_render_input_legacy();
        }
    }
}

Process* shell_get_process() {
    return g_shell_process;
}

// Parse and dispatch a command line
void shell_process_command(const char* cmd) {
    char buf[SHELL_BUFFER_SIZE];
    safe_strncpy(buf, cmd, SHELL_BUFFER_SIZE);
    char* name = strtok(buf, " ");
    char* args = strtok(NULL, "");
    if (!name) return;

    for (size_t i = 0; commands[i].name; ++i) {
        if (strcmp(name, commands[i].name) == 0) {
            commands[i].function(args);
            return;
        }
    }
    printf("Command not found: %s\n", name);
}

// Handle a keyboard event for the shell
void shell_handle_key(keyboard_event ke) {
    if (ke.release) {
        return;
    }
    if (!shell.input_enabled) return;

    const bool graphics = framebuffer::is_available() && g_shell_process != nullptr;

    // Up arrow: previous history
    if (ke.up_arrow) {
        const char* prev = shell_history_prev();
        if (prev) {
            size_t previous_total = shell.prompt_length + shell.length;
            safe_strncpy(shell.buffer, prev, SHELL_BUFFER_SIZE);
            shell.length = strlen(shell.buffer);
            shell.cursor = shell.length;
            if (graphics) {
                shell_render_input();
            } else {
                shell.rendered_chars = previous_total;
                shell_render_input_legacy();
            }
        }
        return;
    }

    // Down arrow: next history
    if (ke.down_arrow) {
        const char* next = shell_history_next();
        size_t previous_total = shell.prompt_length + shell.length;
        if (next && *next) {
            safe_strncpy(shell.buffer, next, SHELL_BUFFER_SIZE);
            shell.length = strlen(shell.buffer);
            shell.cursor = shell.length;
        } else {
            shell.buffer[0] = '\0';
            shell.length = 0;
            shell.cursor = 0;
        }
        if (graphics) {
            shell_render_input();
        } else {
            shell.rendered_chars = previous_total;
            shell_render_input_legacy();
        }
        return;
    }

    if (ke.left_arrow) {
        if (shell.cursor > 0) {
            shell.cursor--;
            if (graphics) {
                shell_render_input();
            } else {
                shell_render_input_legacy();
            }
        }
        return;
    }

    if (ke.right_arrow) {
        if (shell.cursor < shell.length) {
            shell.cursor++;
            if (graphics) {
                shell_render_input();
            } else {
                shell_render_input_legacy();
            }
        }
        return;
    }

    // Backspace
    if (ke.backspace) {
        if (shell.cursor > 0) {
            size_t previous_total = shell.prompt_length + shell.length;
            memmove(&shell.buffer[shell.cursor - 1],
                    &shell.buffer[shell.cursor],
                    shell.length - shell.cursor + 1);
            shell.cursor--;
            if (shell.length > 0) {
                shell.length--;
            }
            if (graphics) {
                shell_render_input();
            } else {
                shell.rendered_chars = previous_total;
                shell_render_input_legacy();
            }
        }
        return;
    }

    // Enter: execute command
    if (ke.enter) {
        shell.buffer[shell.length] = '\0';
        printf("\n");
        shell_history_add(shell.buffer);
        shell_process_command(shell.buffer);
        shell.cursor = 0;
        shell.length = 0;
        shell.buffer[0] = '\0';
        shell.rendered_chars = 0;
        shell_history_reset();
        shell.prompt_visible = false;
        // Only print a new prompt if input is still enabled
        if (shell.input_enabled) {
            shell_print_prompt();
        }
        return;
    }

    // Printable character
    char c = kb_to_ascii(ke);
    if (c && shell.length < SHELL_BUFFER_SIZE - 1) {
        size_t previous_total = shell.prompt_length + shell.length;
        memmove(&shell.buffer[shell.cursor + 1],
                &shell.buffer[shell.cursor],
                shell.length - shell.cursor + 1);
        shell.buffer[shell.cursor] = c;
        shell.cursor++;
        shell.length++;
        if (graphics) {
            shell_render_input();
        } else {
            shell.rendered_chars = previous_total;
            shell_render_input_legacy();
        }
    }
}


// --- Built-in command implementations ---

// Print available commands
void cmd_help(const char* args) {
    (void)args;
    printf("Available commands:\n");
    for (size_t i = 0; commands[i].name; ++i) {
        printf("  %s: %s\n", commands[i].name, commands[i].description);
    }
}

// List directory contents
void cmd_ls(const char* args) {
    const char* path = args && *args ? args : vfs_getcwd();
    
    vfs_dirent_t entries[32];
    int count = vfs_readdir(path, entries, 32);
    
    if (count < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return;
    }
    
    for (int i = 0; i < count; i++) {
        printf("%s%s  ",
               entries[i].name,
               (entries[i].type == VFS_TYPE_DIRECTORY) ? "/" : "");
    }
    printf("\n");
}

// Change directory
void cmd_cd(const char* args) {
    if (!args || !*args) {
        printf("Usage: cd <dir>\n");
        return;
    }
    
    char new_path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        // Build absolute path from current directory
        strcpy(new_path, vfs_getcwd());
        if (strcmp(new_path, "/") != 0) {
            strcat(new_path, "/");
        }
        strcat(new_path, args);
    } else {
        strcpy(new_path, args);
    }
    
    // Validate that the target is a directory
    vfs_dirent_t info;
    if (vfs_stat(new_path, &info) != VFS_SUCCESS) {
        printf("cd: No such file or directory '%s'\n", args);
        return;
    }
    
    if (info.type != VFS_TYPE_DIRECTORY) {
        printf("cd: Not a directory '%s'\n", args);
        return;
    }
    
    // Change directory
    if (vfs_chdir(new_path) != VFS_SUCCESS) {
        printf("cd: Failed to change directory to '%s'\n", args);
    }
}

// Print file contents
void cmd_cat(const char* args) {
    if (!args || !*args) {
        printf("Usage: cat <file>\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        strcpy(path, vfs_getcwd());
        if (strcmp(path, "/") != 0) {
            strcat(path, "/");
        }
        strcat(path, args);
    } else {
        strcpy(path, args);
    }
    
    vfs_file_t file;
    if (vfs_open(path, &file) != VFS_SUCCESS) {
        printf("cat: cannot open '%s': No such file\n", args);
        return;
    }
    
    char buffer[256];
    int bytes_read;
    while ((bytes_read = vfs_read(&file, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    printf("\n");
    
    vfs_close(&file);
}

// Create a new file
void cmd_touch(const char* args) {
    if (!args || !*args) {
        printf("Usage: touch <file>\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        strcpy(path, vfs_getcwd());
        if (strcmp(path, "/") != 0) {
            strcat(path, "/");
        }
        strcat(path, args);
    } else {
        strcpy(path, args);
    }
    
    if (vfs_create(path) != VFS_SUCCESS) {
        printf("touch: cannot create '%s'\n", args);
    } else {
        printf("File '%s' created.\n", args);
    }
}

// Create a new directory
void cmd_mkdir(const char* args) {
    if (!args || !*args) {
        printf("Usage: mkdir <dir>\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        strcpy(path, vfs_getcwd());
        if (strcmp(path, "/") != 0) {
            strcat(path, "/");
        }
        strcat(path, args);
    } else {
        strcpy(path, args);
    }
    
    if (vfs_mkdir(path) != VFS_SUCCESS) {
        printf("mkdir: cannot create directory '%s'\n", args);
    } else {
        printf("Directory '%s' created.\n", args);
    }
}

// Remove a file
void cmd_rm(const char* args) {
    if (!args || !*args) {
        printf("Usage: rm <file>\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        strcpy(path, vfs_getcwd());
        if (strcmp(path, "/") != 0) {
            strcat(path, "/");
        }
        strcat(path, args);
    } else {
        strcpy(path, args);
    }
    
    if (vfs_remove(path) != VFS_SUCCESS) {
        printf("rm: cannot remove '%s'\n", args);
    } else {
        printf("File '%s' removed.\n", args);
    }
}

// Remove a directory
void cmd_rmdir(const char* args) {
    if (!args || !*args) {
        printf("Usage: rmdir <dir>\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    
    // Handle relative paths
    if (args[0] != '/') {
        strcpy(path, vfs_getcwd());
        if (strcmp(path, "/") != 0) {
            strcat(path, "/");
        }
        strcat(path, args);
    } else {
        strcpy(path, args);
    }
    
    if (vfs_rmdir(path) != VFS_SUCCESS) {
        printf("rmdir: cannot remove directory '%s'\n", args);
    } else {
        printf("Directory '%s' removed.\n", args);
    }
}

// Print text to terminal
void cmd_echo(const char* args) {
    printf("%s\n", args ? args : "");
}

// Print working directory
void cmd_pwd(const char* args) {
    (void)args;
    printf("%s\n", vfs_getcwd());
}

// Print system uptime
void cmd_uptime(const char* args) {
    (void)args;
    uint32_t ticks = get_ticks();
    printf("Uptime: %u ms\n", ticks);
}

// Print command history
void cmd_history(const char* args) {
    (void)args;
    unsigned hcount = (unsigned)shell.history_count;
    unsigned hsize  = SHELL_HISTORY_SIZE;
    unsigned start  = (hcount > hsize ? hcount - hsize : 0u);

    for (unsigned i = start; i < hcount; ++i) {
        unsigned num = i + 1u;
        unsigned idx = i % hsize;
        printf("%u: %s\n", num, shell.history[idx]);
    }
}

// Open file in editor
void cmd_edit(const char* args) {
    if (!args || !*args) {
        printf("Usage: edit <path>\n");
        return;
    }

    char combined[VFS_MAX_PATH];
    if (args[0] == '/') {
        strncpy(combined, args, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    } else {
        const char* cwd = vfs_getcwd();
        size_t cwd_len = strlen(cwd);
        size_t pos = 0;
        if (cwd_len >= sizeof(combined)) cwd_len = sizeof(combined) - 1;
        memcpy(combined, cwd, cwd_len);
        pos = cwd_len;
        if (pos == 0 || combined[pos - 1] != '/') {
            if (pos + 1 < sizeof(combined)) {
                combined[pos++] = '/';
            }
        }
        size_t remaining = (sizeof(combined) > pos) ? (sizeof(combined) - pos - 1) : 0;
        size_t arg_len = strlen(args);
        if (arg_len > remaining) {
            arg_len = remaining;
        }
        memcpy(combined + pos, args, arg_len);
        combined[pos + arg_len] = '\0';
    }

    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(combined, normalized) != VFS_SUCCESS) {
        printf("edit: failed to resolve path '%s'\n", combined);
        return;
    }

    vfs_dirent_t info;
    if (vfs_stat(normalized, &info) == VFS_SUCCESS && info.type == VFS_TYPE_DIRECTORY) {
        printf("edit: '%s' is a directory\n", normalized);
        return;
    }

    char parent[VFS_MAX_PATH];
    strncpy(parent, normalized, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char* slash = strrchr(parent, '/');
    if (slash) {
        if (slash == parent) {
            if (*(slash + 1) == '\0') {
                strcpy(parent, "/");
            } else {
                slash[1] = '\0';
            }
        } else {
            *slash = '\0';
        }
    }
    if (parent[0] == '\0') {
        strcpy(parent, "/");
    }

    vfs_dirent_t parent_info;
    if (vfs_stat(parent, &parent_info) != VFS_SUCCESS || parent_info.type != VFS_TYPE_DIRECTORY) {
        printf("edit: parent directory '%s' not found\n", parent);
        return;
    }

    editor_set_params(normalized);
    Process* p = k_start_process("editor", editor_entry, 0, 8192);
    if (!p) {
        printf("edit: failed to start editor process\n");
        return;
    }

    scheduler_set_foreground(p);
    shell_set_input_enabled(false);
}

// List block devices
void cmd_lsblk(const char* args) {
    (void)args;
    blockdev_list_devices();
}

// Test disk read
void cmd_disktest(const char* args) {
    (void)args;
    printf("Testing disk read...\n");
    
    uint8_t buffer[512];
    int result = blockdev_read(0, 0, 1, buffer);
    
    if (result == 0) {
        printf("Successfully read sector 0:\n");
        // Print first 64 bytes as hex
        for (int i = 0; i < 64; i++) {
            if (i % 16 == 0) printf("\n%04x: ", i);
            printf("%02x ", buffer[i]);
        }
        printf("\n");
        
        // Print as text
        printf("As text: ");
        for (int i = 0; i < 64; i++) {
            char c = buffer[i];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    } else {
        printf("Failed to read disk: error %d\n", result);
    }
}

// Mount filesystem command
void cmd_mount(const char* args) {
    if (!args || !*args) {
        printf("Current mounts:\n");
        vfs_list_mounts();
        printf("\nUsage: mount fat32 - Mount FAT32 from device 0 to /mnt/fat32\n");
        return;
    }
    
    if (strcmp(args, "fat32") == 0) {
        if (fat32_vfs_mount("/mnt/fat32", 0) == VFS_SUCCESS) {
            printf("FAT32 filesystem mounted at /mnt/fat32\n");
        } else {
            printf("Failed to mount FAT32 filesystem\n");
        }
    } else {
        printf("Unknown filesystem type: %s\n", args);
        printf("Supported types: fat32\n");
    }
}

// Unmount filesystem
void cmd_umount(const char* args) {
    if (!args || !*args) {
        printf("Usage: umount <mountpoint>\n");
        return;
    }
    
    if (vfs_unmount(args) == VFS_SUCCESS) {
        printf("Filesystem unmounted from %s\n", args);
    } else {
        printf("Failed to unmount %s\n", args);
    }
}

// Show FAT32 filesystem info
void cmd_fat32_info(const char* args) {
    (void)args;
    fat32_get_fs_info();
}

// Print memory usage information
void cmd_meminfo(const char* args) {
    (void)args;
    
    // Physical memory statistics
    uint32_t total_mem = PhysicalMemoryManager::get_memory_size();
    uint32_t free_frames = PhysicalMemoryManager::get_free_frames();
    uint32_t used_frames = PhysicalMemoryManager::used_frames;
    uint32_t free_mem = free_frames * PAGE_SIZE;
    uint32_t used_mem = used_frames * PAGE_SIZE;
    
    // Heap statistics
    heap_stats_t heap_stats;
    get_heap_stats(&heap_stats);
    
    printf("\n=== Physical Memory Information ===\n");
    printf("Total Memory:        %u bytes (%u MB)\n", total_mem, total_mem / (1024 * 1024));
    printf("Used Memory:         %u bytes (%u MB)\n", used_mem, used_mem / (1024 * 1024));
    printf("Free Memory:         %u bytes (%u MB)\n", free_mem, free_mem / (1024 * 1024));
    printf("Total Frames:        %u frames\n", used_frames + free_frames);
    printf("Used Frames:         %u frames\n", used_frames);
    printf("Free Frames:         %u frames\n", free_frames);
    printf("Frame Size:          %u bytes\n", PAGE_SIZE);
    printf("Memory Usage:        %u%%\n", (used_mem * 100) / total_mem);
    
    printf("\n=== Kernel Heap Information ===\n");
    printf("Heap Start:          0x%x\n", KERNEL_HEAP_START);
    printf("Heap Size:           %u bytes (%u MB)\n", KERNEL_HEAP_SIZE, KERNEL_HEAP_SIZE / (1024 * 1024));
    printf("Total Heap:          %u bytes\n", heap_stats.total_size);
    printf("Used Heap:           %u bytes\n", heap_stats.used_size);
    printf("Free Heap:           %u bytes\n", heap_stats.free_size);
    printf("Metadata Overhead:   %u bytes\n", heap_stats.overhead);
    printf("Allocated Blocks:    %u blocks\n", heap_stats.allocated_blocks);
    printf("Free Blocks:         %u blocks\n", heap_stats.free_blocks);
    printf("Largest Free Block:  %u bytes\n", heap_stats.largest_free_block);
    printf("Heap Usage:          %u%%\n", 
           heap_stats.total_size > 0 ? (heap_stats.used_size * 100) / heap_stats.total_size : 0);
    
    printf("\n=== Memory Layout ===\n");
    printf("Kernel Heap:         0x%x - 0x%x\n", 
           KERNEL_HEAP_START, KERNEL_HEAP_START + KERNEL_HEAP_SIZE);
    printf("Page Size:           %u bytes\n", PAGE_SIZE);
    
    printf("\n");
}

// Print free memory (Linux-style free command)
void cmd_free(const char* args) {
    (void)args;
    
    // Physical memory statistics
    uint32_t total_mem = PhysicalMemoryManager::get_memory_size();
    uint32_t free_frames = PhysicalMemoryManager::get_free_frames();
    uint32_t used_frames = PhysicalMemoryManager::used_frames;
    uint32_t free_mem = free_frames * PAGE_SIZE;
    uint32_t used_mem = used_frames * PAGE_SIZE;
    
    // Heap statistics
    heap_stats_t heap_stats;
    get_heap_stats(&heap_stats);
    
    // Display in a format similar to Linux 'free' command (without width specifiers)
    printf("            total        used        free\n");
    printf("Mem:   %u  %u  %u\n", total_mem, used_mem, free_mem);
    printf("Heap:  %u  %u  %u\n", heap_stats.total_size, heap_stats.used_size, heap_stats.free_size);
    printf("\n");
    printf("Memory usage: %u%% (Physical), %u%% (Heap)\n",
           (used_mem * 100) / total_mem,
           heap_stats.total_size > 0 ? (heap_stats.used_size * 100) / heap_stats.total_size : 0);
}

// List PCI devices
void cmd_lspci(const char* args) {
    (void)args;
    pci_list_devices();
}

// Command lookup table
shell_command_t commands[] = {
    { "help",     cmd_help,     "Show available commands" },
    { "ls",       cmd_ls,       "List directory contents" },
    { "cd",       cmd_cd,       "Change directory" },
    { "cat",      cmd_cat,      "Display file contents" },
    { "touch",    cmd_touch,    "Create a new file" },
    { "mkdir",    cmd_mkdir,    "Create a new directory" },
    { "rm",       cmd_rm,       "Remove a file" },
    { "rmdir",    cmd_rmdir,    "Remove a directory" },
    { "echo",     cmd_echo,     "Print text" },
    { "pwd",      cmd_pwd,      "Print working directory" },
    { "uptime",   cmd_uptime,   "Show system uptime" },
    { "history",  cmd_history,  "Show command history" },
    { "edit",      cmd_edit,      "Edit a file" },
    { "lsblk",     cmd_lsblk,     "List block devices" },
    { "disktest",  cmd_disktest,  "Test disk reading" },
    { "mount",     cmd_mount,      "Mount filesystem" },
    { "umount",    cmd_umount,     "Unmount filesystem" },
    { "fsinfo",    cmd_fat32_info, "Show filesystem info" },
    { "meminfo",   cmd_meminfo,    "Show detailed memory usage" },
    { "free",      cmd_free,       "Display memory usage summary" },
    { "lspci",     cmd_lspci,      "List PCI devices" },
    { NULL,        NULL,          NULL }
};

extern "C" void shell_entry() {
    Process* proc = scheduler_current_process();
    if (proc) {
        g_shell_process = proc;
        scheduler_set_foreground(proc);
    }
    shell_init();
    IOEvent event;
    while (1) {
        if (!process_poll_event(&event)) {
            if (!process_wait_event(&event)) {
                continue;
            }
        }
        if (event.type == EVENT_PROCESS) {
            if (event.data.process.code == PROCESS_EVENT_FOCUS_LOST) {
                shell_set_input_enabled(false);
            } else if (event.data.process.code == PROCESS_EVENT_FOCUS_GAINED) {
                shell_set_input_enabled(true);
                if (!shell.prompt_visible) {
                    shell_print_prompt();
                }
            }
            continue;
        }
        if (event.type == EVENT_KEYBOARD) {
            shell_handle_key(event.data.keyboard);
            continue;
        }
    }
}
