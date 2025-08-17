#include <stdio.h>
#include <stdint.h>
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/syscalls.h"
#include "kernel/timer.h"

static volatile int g_should_quit = 0;

static void on_key(keyboard_event ev) {
    if (!ev.release) {
        g_should_quit = 1;
    }
}

extern "C" void test_proc_entry() {
    Process* self = scheduler_current_process();
    if (self) {
        register_keyboard_handler(self, on_key);
    }

    printf("[test-proc] Started. Press any key to quit...\n");

    // Idle until a keypress arrives
    while (!g_should_quit) {
        // Sleep cooperatively by yielding until next tick
        uint32_t target = get_ticks() + 1; // next tick
        sys_yield_for_event((int)HookType::TIME_REACHED, target);
    }

    printf("[test-proc] Quitting on key press. Bye!\n");

    // Request process exit via syscall 0x83
    asm volatile("mov $0x83, %%eax; int $0x80" ::: "eax", "memory");

    // Should not run anymore; idle safely
    while (1) asm volatile("hlt");
}
