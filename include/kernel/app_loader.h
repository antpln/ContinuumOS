#ifndef KERNEL_APP_LOADER_H
#define KERNEL_APP_LOADER_H

#include <stddef.h>

struct Process;

typedef struct AppLoadParams
{
    const char* entry_symbol;   // Required entry point symbol (e.g., "editor_entry")
    const char* init_symbol;    // Optional initializer symbol (e.g., "editor_set_params")
    size_t stack_size;          // Stack size for the process (default if zero)
} AppLoadParams;

// Load an application image from `path`, resolve symbols, run constructors,
// and start it as a kernel-managed process. Returns nullptr on error.
Process* app_load_and_start(const char* path,
                            const char* process_name,
                            const AppLoadParams* params,
                            const char* init_argument);

#endif // KERNEL_APP_LOADER_H
