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
#include "editor_process.h"
#include <kernel/process.h>
#include <kernel/blockdev.h>
#include <kernel/fat32.h>
#include <kernel/memory.h>
#include <kernel/scheduler.h>
#include <process.h>

extern Terminal terminal;
extern shell_command_t commands[];

#define SHELL_BUFFER_SIZE    256
#define SHELL_HISTORY_SIZE   16
#define SHELL_PROMPT         "nutshell> "
#define SHELL_PROMPT_LEN     (sizeof(SHELL_PROMPT) - 1)

#define SHELL_PWD_MAX_DEPTH  32

static Process* g_shell_process = nullptr;

typedef struct {
    char     buffer[SHELL_BUFFER_SIZE];
    size_t   index;
    bool     input_enabled;
    bool     prompt_visible;
    char     history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
    size_t   history_count;
    int      history_nav;
} ShellState;

static ShellState shell;

// Copy src into dst[n], always null-terminated
static inline void safe_strncpy(char* dst, const char* src, size_t n) {
    strncpy(dst, src, n);
    dst[n - 1] = '\0';
}

// Clear `length` characters from the terminal line
static void clear_line(size_t length) {
    for (size_t i = 0; i < length; ++i) printf("\b");
    for (size_t i = 0; i < length; ++i) printf(" ");
    for (size_t i = 0; i < length; ++i) printf("\b");
}

// Print the shell prompt
static void shell_print_prompt(void) {
    printf("%s", SHELL_PROMPT);
    shell.prompt_visible = true;
}

// Initialize shell state and print welcome message
void shell_init(void) {
    shell.index         = 0;
    shell.input_enabled = true;
    shell.prompt_visible = false;
    shell.history_count = 0;
    shell.history_nav   = -1;
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

    // Up arrow: previous history
    if (ke.up_arrow) {
        const char* prev = shell_history_prev();
        if (prev) {
            size_t len = SHELL_PROMPT_LEN + shell.index;
            clear_line(len);
            safe_strncpy(shell.buffer, prev, SHELL_BUFFER_SIZE);
            shell.index = strlen(shell.buffer);
            shell_print_prompt();
            printf("%s", shell.buffer);
        }
        return;
    }

    // Down arrow: next history
    if (ke.down_arrow) {
        const char* next = shell_history_next();
        size_t len = SHELL_PROMPT_LEN + shell.index;
        clear_line(len);
        if (next && *next) {
            safe_strncpy(shell.buffer, next, SHELL_BUFFER_SIZE);
            shell.index = strlen(shell.buffer);
            shell_print_prompt();
            printf("%s", shell.buffer);
        } else {
            shell.buffer[0] = '\0';
            shell.index = 0;
            shell_print_prompt();
        }
        return;
    }

    // Backspace
    if (ke.backspace) {
        if (shell.index > 0) {
            shell.index--;
            printf("\b \b");
        }
        return;
    }

    // Enter: execute command
    if (ke.enter) {
        shell.buffer[shell.index] = '\0';
        printf("\n");
        shell_history_add(shell.buffer);
        shell_process_command(shell.buffer);
        shell.index = 0;
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
    if (c && shell.index < SHELL_BUFFER_SIZE - 1) {
        shell.buffer[shell.index++] = c;
        printf("%c", c);
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
        printf("Usage: edit <file>\n");
        return;
    }

    char path[VFS_MAX_PATH];
    if (args[0] != '/') {
        const char* cwd = vfs_getcwd();
        size_t cwd_len = strlen(cwd);
        size_t args_len = strlen(args);
        size_t extra = (strcmp(cwd, "/") == 0) ? 0 : 1;
        if (cwd_len + extra + args_len >= sizeof(path)) {
            printf("edit: path too long\n");
            return;
        }
        strcpy(path, cwd);
        if (extra) {
            path[cwd_len] = '/';
            path[cwd_len + 1] = '\0';
        }
        strcat(path, args);
    } else {
        if (strlen(args) >= sizeof(path)) {
            printf("edit: path too long\n");
            return;
        }
        strncpy(path, args, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    vfs_dirent_t info;
    if (vfs_stat(path, &info) != VFS_SUCCESS) {
        if (vfs_create(path) != VFS_SUCCESS) {
            printf("edit: cannot create '%s'\n", path);
            return;
        }
    } else if (info.type != VFS_TYPE_FILE) {
        printf("edit: '%s' is not a file\n", path);
        return;
    }

    editor_set_params(path);
    Process* p = k_start_process("editor", editor_entry, 0, 8192);
    if (p) {
        scheduler_set_foreground(p);
    }
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

// List FAT32 root directory (legacy command - use ls /mnt/fat32 instead)
void cmd_fat32_ls(const char* args) {
    (void)args;
    printf("Use 'ls /mnt/fat32' instead\n");
}

// Read and display FAT32 file (legacy command - use cat /mnt/fat32/filename instead)
void cmd_fat32_cat(const char* args) {
    if (!args || !*args) {
        printf("Usage: fat32cator <filename>\n");
        printf("Note: Use 'cat /mnt/fat32/<filename>' instead\n");
        return;
    }
    
    char path[VFS_MAX_PATH];
    strcpy(path, "/mnt/fat32/");
    strcat(path, args);
    
    printf("Reading FAT32 file via VFS: %s\n", path);
    cmd_cat(path);
}

// Print memory usage information
void cmd_meminfo(const char* args) {
    (void)args;
    uint32_t total = PhysicalMemoryManager::get_memory_size();
    uint32_t free = PhysicalMemoryManager::get_free_frames() * PAGE_SIZE;
    printf("Total memory: %u bytes\n", total);
    printf("Free memory:  %u bytes\n", free);
    printf("Used frames:  %u\n", PhysicalMemoryManager::used_frames);
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
    { "fat32ls",   cmd_fat32_ls,   "List FAT32 root directory (legacy)" },
    { "fat32cat",  cmd_fat32_cat,  "Read and display FAT32 file (legacy)" },
    { "meminfo", cmd_meminfo, "Show memory usage info" },
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
