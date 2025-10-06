#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kernel/keyboard.h>
#include <kernel/isr.h>
#include <kernel/timer.h>
#include <kernel/ramfs.h>
#include <kernel/heap.h>
#include <kernel/vga.h>      
#include <kernel/shell.h>
#include "editor_process.h"
#include <kernel/process.h>
#include "kernel/memory.h"
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
    FSNode*  cwd;
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
    shell.cwd           = fs_get_root();
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
    (void)args;
    if (!shell.cwd) shell.cwd = fs_get_root();
    for (size_t i = 0; i < shell.cwd->child_count; ++i) {
        FSNode* n = shell.cwd->children[i];
        printf("%s%s  ",
               n->name,
               (n->type == FS_DIRECTORY) ? "/" : "");
    }
    printf("\n");
}

// Change directory
void cmd_cd(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: cd <dir>\n");
        return;
    }
    if (strcmp(args, "..") == 0) {
        if (shell.cwd->parent) shell.cwd = shell.cwd->parent;
        else printf("Already at root.\n");
        return;
    }
    FSNode* target = fs_find_child(shell.cwd, args);
    if (target && target->type == FS_DIRECTORY)
        shell.cwd = target;
    else
        printf("cd: No such directory '%s'\n", args);
}

// Print file contents
void cmd_cat(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: cat <file>\n");
        return;
    }
    FSNode* f = fs_find_child(shell.cwd, args);
    if (f && f->type == FS_FILE)
        printf("%s\n", (char*)f->data);
    else
        printf("cat: No such file '%s'\n", args);
}

// Create a new file
void cmd_touch(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: touch <file>\n");
        return;
    }
    FSNode* f = fs_create_node(args, FS_FILE);
    if (!f) {
        printf("Error: could not create file '%s'\n", args);
        return;
    }
    f->size = 0;
    f->data = NULL;
    fs_add_child(shell.cwd, f);
    printf("File '%s' created.\n", args);
}

// Create a new directory
void cmd_mkdir(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: mkdir <dir>\n");
        return;
    }
    FSNode* d = fs_create_node(args, FS_DIRECTORY);
    if (!d) {
        printf("Error: could not create directory '%s'\n", args);
        return;
    }
    fs_add_child(shell.cwd, d);
    printf("Directory '%s' created.\n", args);
}

// Remove a file
void cmd_rm(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: rm <file>\n");
        return;
    }
    FSNode* f = fs_find_child(shell.cwd, args);
    if (f && f->type == FS_FILE) {
        fs_remove_child(shell.cwd, f);
        printf("File '%s' removed.\n", args);
    } else {
        printf("rm: No such file '%s'\n", args);
    }
}

// Remove a directory
void cmd_rmdir(const char* args) {
    if (!shell.cwd) shell.cwd = fs_get_root();
    if (!args || !*args) {
        printf("Usage: rmdir <dir>\n");
        return;
    }
    FSNode* d = fs_find_child(shell.cwd, args);
    if (d && d->type == FS_DIRECTORY) {
        fs_remove_child(shell.cwd, d);
        printf("Directory '%s' removed.\n", args);
    } else {
        printf("rmdir: No such directory '%s'\n", args);
    }
}

// Print text to terminal
void cmd_echo(const char* args) {
    printf("%s\n", args ? args : "");
}

// Print working directory
void cmd_pwd(const char* args) {
    (void)args;
    if (!shell.cwd) shell.cwd = fs_get_root();
    const char* names[SHELL_PWD_MAX_DEPTH];
    size_t depth = 0;
    for (FSNode* node = shell.cwd; node && node->parent; node = node->parent) {
        if (depth < SHELL_PWD_MAX_DEPTH) {
            names[depth++] = node->name;
        } else {
            break;
        }
    }
    printf("/");
    for (size_t i = 0; i < depth; ++i) {
        printf("%s", names[depth - i - 1]);
        if (i + 1 < depth) {
            printf("/");
        }
    }
    printf("\n");
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
        printf("Usage: editor <file>\n");
        return;
    }
    if (!shell.cwd) shell.cwd = fs_get_root();
    // Set parameters safely (copy) before starting the process
    editor_set_params(args, shell.cwd);
    Process* p = k_start_process("editor", editor_entry, 0, 8192);
    if (p) {
        // Give keyboard focus to the editor immediately and disable shell input
        scheduler_set_foreground(p);
    }
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
    { "help",    cmd_help,    "Show available commands" },
    { "ls",      cmd_ls,      "List directory contents" },
    { "cd",      cmd_cd,      "Change directory" },
    { "cat",     cmd_cat,     "Display file contents" },
    { "touch",   cmd_touch,   "Create a new file" },
    { "mkdir",   cmd_mkdir,   "Create a new directory" },
    { "rm",      cmd_rm,      "Remove a file" },
    { "rmdir",   cmd_rmdir,   "Remove a directory" },
    { "echo",    cmd_echo,    "Print text" },
    { "pwd",     cmd_pwd,     "Print working directory" },
    { "uptime",  cmd_uptime,  "Show system uptime" },
    { "history", cmd_history, "Show command history" },
    { "editor",  cmd_edit,    "Edit a file" },
    { "meminfo", cmd_meminfo, "Show memory usage info" },
    { NULL,      NULL,        NULL }
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
