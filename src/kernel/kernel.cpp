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
#include "kernel/vfs.h"
#include "kernel/debug.h"
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

		// Initialize the Physical Memory Manager (PMM) before paging
		PhysicalMemoryManager::initialize(multiboot_info);

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
		// Initialize VFS (Virtual File System)
		vfs_init();
		// Mount RamFS at root
		ramfs_vfs_mount("/");
		// Create /mnt directory for mount points
		debug("Creating /mnt directory...");
		if (vfs_mkdir("/mnt") == VFS_SUCCESS) {
			success("/mnt directory created successfully");
		} else {
			error("Failed to create /mnt directory");
		}
		// Try to mount FAT32 if available
		fat32_vfs_mount("/mnt/fat32", 0);
		// Create some built-in files using VFS
		debug("Creating /README file via VFS...");
		if (vfs_create("/README") == VFS_SUCCESS) {
			success("README file created successfully");
			
			// Write content to the file
			vfs_file_t file;
			if (vfs_open("/README", &file) == VFS_SUCCESS) {
				const char *msg = "Welcome to ContinuumOS!";
				int bytes_written = vfs_write(&file, msg, strlen(msg));
				debug("Wrote %d bytes to README", bytes_written);
				vfs_close(&file);
			}
		} else {
			error("Failed to create README file");
		}
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
