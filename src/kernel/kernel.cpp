#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel/vga.h"
#include "kernel/idt.h"
#include "kernel/memory.h"
#include "kernel/paging.h"
#include "kernel/heap.h"
#include "kernel/pic.h"
#include "kernel/keyboard.h"
#include "kernel/gdt.h"
#include "kernel/timer.h"
#include "kernel/shell.h"
#include "kernel/ramfs.h"
#include "kernel/debug.h"
#include "kernel/tests/memtest.h"
#include "kernel/tests/pagetest.h"
#include "kernel/tests/heaptest.h"
#include "kernel/scheduler.h"
#include <kernel/process.h>

#include "utils.h"
#include <stdio.h> // Changed back to just stdio.h since include path is set in Makefile

#ifdef __cplusplus
extern "C"

{
        void shell_entry();
#endif

	Terminal terminal;

	void kernel_main(uint32_t multiboot_info)
	{

		char *ascii_guitar = R"(
          Q
         /|\
       (o\_)=="#
        \| |\
       ~H| |/
            ~)";
		terminal.initialize();

		// Initialize the scheduler (round-robin)
		scheduler_init();

		// Initialize the Physical Memory Manager (PMM) before paging
		PhysicalMemoryManager::initialize(multiboot_info);

		init_gdt(); // sets up GDT and flushes it

		// Remap the PIC
		init_pic();

		// Initialize the IDT
		init_idt();
		init_syscall_handler();

		// Initialize virtual memory management
		vmm_init();
		vmm_enable();

		// Set up heap
		init_heap();

		// Initialize the RAMFS.
		fs_init();

		#ifdef TEST
		MemoryTester mem_tester;
		if (!mem_tester.test_allocation()) {
			PANIC("Memory allocation test failed!");
		} else {
			success("Memory allocation test passed!");
		}
		if (!mem_tester.test_free()) {
			PANIC("Memory free test failed!");
		} else {
			success("Memory free test passed!");
		}
		if (!mem_tester.test_multiple_allocations()) {
			PANIC("Memory multiple allocations test failed!");
		} else {
			success("Memory multiple allocations test passed!");
		}
		paging_test();
		#endif

		// Create some built-in files or directories.
		FSNode *root = fs_get_root();
		FSNode *readme = fs_create_node("README", FS_FILE);
		if (readme)
		{
			// Allocate a buffer for the file content.
			readme->size = 128;
			readme->data = (uint8_t *)kmalloc(readme->size);
			if (readme->data)
			{
				// Write some content into the file.
				const char *msg = "Welcome to ContinuumOS!";
				strncpy((char *)readme->data, msg, readme->size);
			}
			fs_add_child(root, readme);
		}

                keyboard_install();
                // Initialize the PIT timer to 1000 Hz
                init_timer(1000);

                Process* shell_proc = start_process("shell", shell_entry, 0, 8192);
                (void)shell_proc;

                __asm__ volatile("sti");

                scheduler_start();
	}

#ifdef __cplusplus
}
#endif
