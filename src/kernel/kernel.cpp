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
#include "kernel/tests/memtest.h"
#include "kernel/tests/pagetest.h"
#include "kernel/tests/heaptest.h"
#include "kernel/blockdev.h"
#include "kernel/fat32.h"

#include "utils.h"
#include <stdio.h> // Changed back to just stdio.h since include path is set in Makefile

#ifdef __cplusplus
extern "C"

{
#endif

	Terminal terminal;

	void kernel_main(uint32_t multiboot_info)
	{
		(void)multiboot_info; // Suppress unused parameter warning
		
		terminal.initialize();

		init_gdt(); // sets up GDT and flushes it

		// Remap the PIC
		init_pic();

		// Initialize the IDT
		init_idt();

		// Initialize virtual memory management
		vmm_init();
		vmm_enable();

		// Set up heap
		init_heap();

		// Initialize block devices (IDE, etc.)
		blockdev_init();

		// Initialize FAT32 support
		fat32_init();

		// Initialize the RAMFS.
		fs_init();

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

		shell_init();

		__asm__ volatile("sti");

		while (1)
		{
			__asm__ volatile("hlt");
		}
	}

#ifdef __cplusplus
}
#endif
