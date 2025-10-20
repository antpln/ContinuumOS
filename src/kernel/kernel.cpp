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
#include "kernel/mouse.h"
#include "kernel/gdt.h"
#include "kernel/timer.h"
#include "kernel/shell.h"
#include "kernel/ramfs.h"
#include "kernel/vfs.h"
#include "kernel/debug.h"
#include "kernel/serial.h"
#include "kernel/tests/memtest.h"
#include "kernel/tests/pagetest.h"
#include "kernel/tests/heaptest.h"
#include "kernel/scheduler.h"
#include <kernel/process.h>
#include "kernel/blockdev.h"
#include "kernel/fat32.h"
#include "kernel/multiboot.h"
#include "kernel/framebuffer.h"
#include "kernel/gui.h"
#include "kernel/terminal_windows.h"
#include "kernel/pci.h"

#include "utils.h"
#include <stdio.h> // Changed back to just stdio.h since include path is set in Makefile

#ifdef __cplusplus
extern "C"

{
	void shell_entry();
	void test_proc_entry();
#endif

		Terminal terminal;

		void kernel_main(uint32_t multiboot_info_ptr)
		{
			serial_init();
			multiboot_info_t *mb_info = reinterpret_cast<multiboot_info_t *>(multiboot_info_ptr);
			bool framebuffer_ready = framebuffer::initialize(mb_info);
			if (framebuffer_ready)
			{
				debug("Framebuffer ready (%ux%u@%u) double buffering %s",
					  framebuffer::info().width,
					  framebuffer::info().height,
					  framebuffer::info().bpp,
					  framebuffer::double_buffering_enabled() ? "on" : "off");
			}

		const char *ascii_guitar = R"(
          Q
         /|\
       (o\_)=="#
        \| |\
       ~H| |/
            ~)";
		(void)ascii_guitar;
		debug("ContinuumOS Kernel Starting...");
		terminal.initialize();
		if (!framebuffer_ready)
		{
			serial_write("[WARN] Framebuffer not available; using legacy text mode\n");
			terminal.writeLine("[WARN] Framebuffer not available; using text mode");
		}

		// Initialize the scheduler (round-robin)
		scheduler_init();

		// Initialize the Physical Memory Manager (PMM) before paging
		PhysicalMemoryManager::initialize(multiboot_info_ptr);

		init_gdt(); // sets up GDT and flushes it

		// Remap the PIC
		init_pic();

		// Initialize the IDT
		init_idt();
		init_syscall_handler();

		mouse_initialize();

		uint32_t fb_phys = 0;
		uint32_t fb_size = 0;
		if (framebuffer_ready)
		{
			fb_phys = framebuffer::framebuffer_physical_address();
			fb_size = framebuffer::framebuffer_size();
		}

		// Initialize virtual memory management
		vmm_init();

		if (framebuffer_ready && fb_phys != 0 && fb_size != 0)
		{
			uint32_t phys_aligned = fb_phys & ~(PAGE_SIZE - 1);
			uint32_t offset = fb_phys - phys_aligned;
			uint32_t map_size = fb_size + offset;
			vmm_map_range(fb_phys - offset, phys_aligned, map_size, 1);
		}

		vmm_enable();

		if (framebuffer_ready)
		{
			terminal.set_graphics_origin(48, 96);
			terminal_windows::init(terminal, nullptr);
			gui::draw_boot_screen();
			gui::draw_workspace(terminal);
			terminal.refresh();
		}

		// Set up heap
		init_heap();

		// Initialize block devices (IDE, etc.)
		blockdev_init();

		// Initialize PCI subsystem
		pci_init();

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
		if (vfs_mkdir("/mnt") == VFS_SUCCESS)
		{
			success("/mnt directory created successfully");
		}
		else
		{
			error("Failed to create /mnt directory");
		}
		// Try to mount FAT32 if available
		fat32_vfs_mount("/mnt/fat32", 0);
		// Create some built-in files using VFS
		debug("Creating /README file via VFS...");
		if (vfs_create("/README") == VFS_SUCCESS)
		{
			success("README file created successfully");

			// Write content to the file
			vfs_file_t file;
			if (vfs_open("/README", &file) == VFS_SUCCESS)
			{
				const char *msg = "Welcome to ContinuumOS!";
				int bytes_written = vfs_write(&file, msg, strlen(msg));
				debug("Wrote %d bytes to README", bytes_written);
				vfs_close(&file);
			}
		}
		else
		{
			error("Failed to create README file");
		}
#ifdef TEST
		MemoryTester mem_tester;
		if (!mem_tester.test_allocation())
		{
			PANIC("Memory allocation test failed!");
		}
		else
		{
			success("Memory allocation test passed!");
		}
		if (!mem_tester.test_free())
		{
			PANIC("Memory free test failed!");
		}
		else
		{
			success("Memory free test passed!");
		}
		if (!mem_tester.test_multiple_allocations())
		{
			PANIC("Memory multiple allocations test failed!");
		}
		else
		{
			success("Memory multiple allocations test passed!");
		}
		paging_test();
#endif

		// Create some built-in files or directories.
		fs_get_root();
		keyboard_install();
		// Initialize the PIT timer to 1000 Hz
		init_timer(1000);

		if (framebuffer_ready)
		{
			// In graphics mode, start the shell and give it a window.
			// Other processes (like the editor) will run in the same context.
			Process *shell_proc = k_start_process("shell", shell_entry, 0, 8192);
			terminal_windows::request_new_window(terminal, shell_proc);
			gui::draw_workspace(terminal);
		} else {
			// In text mode, just start the shell process.
			k_start_process("shell", shell_entry, 0, 8192);
		}

		__asm__ volatile("sti");

		scheduler_start();
	}

#ifdef __cplusplus
}
#endif
