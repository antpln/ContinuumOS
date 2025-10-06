#include <stdio.h>
#include <stdint.h>
#include "kernel/hooks.h"
#include "kernel/timer.h"
#include <process.h>

extern "C" void test_proc_entry() {
    printf("[test-proc] Started. Press any key to quit...\n");

    bool quitting = false;
    IOEvent event;
    while (!quitting) {
        if (process_poll_event(&event)) {
            if (event.type == EVENT_KEYBOARD && !event.data.keyboard.release) {
                quitting = true;
                continue;
            }
        }
        uint32_t target = get_ticks() + 1; // next tick
        yield_for_event((int)HookType::TIME_REACHED, target);
    }

    printf("[test-proc] Quitting on key press. Bye!\n");

    process_exit(0);

    // Should not run anymore; idle safely
    while (1) asm volatile("hlt");
}
