#ifndef SHELL_H
#define SHELL_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kernel/keyboard.h>
#include <kernel/isr.h>

struct Process;

typedef void (*command_func_t)(const char* args);

typedef struct shell_command_t {
    const char* name;
    command_func_t function;
    const char* description;
} shell_command_t;

void cmd_help(const char* args);
void cmd_mount(const char* args);
void cmd_umount(const char* args);

// Process a completed command line.
void shell_process_command(const char* cmd);

// This function is called by the keyboard interrupt handler whenever a valid character arrives.
void shell_handle_key(keyboard_event ke);

// Initialize the shell (print a welcome message and the prompt).
void shell_init();

// Enable or disable shell input (used when a foreground app takes focus)
void shell_set_input_enabled(bool enabled);

// Retrieve the shell process instance (if running)
Process* shell_get_process();

#endif // SHELL_H
