# ContinuumOS

ContinuumOS is a light-weight minimal i686 kernel for training purposes.

## Features



### Core System
*   32-bit i686 Kernel
*   VGA Text Mode Driver
*   Keyboard Driver
*   Interrupt Descriptor Table (IDT) and Interrupt Service Routines (ISR)
*   Paging and Memory Management
*   Kernel Heap
*   Multitasking with Process Management
*   Cooperative and Preemptive Scheduling
*   Lottery-based Process Scheduling
*   System Calls Interface

### Hardware Support
*   Programmable Interrupt Controller (PIC)
*   Programmable Interval Timer (PIT)
*   IDE/ATA Hard Disk Driver (PIO mode)
*   Block Device Abstraction Layer

### Filesystems
*   RamFS (simple RAM-based filesystem)
*   FAT32 Filesystem (read-only support)
*   Virtual File System (VFS) abstraction layer
*   File operations: open, read, seek, close
*   Directory listing and navigation

### User Interface
*   Interactive shell with command history
*   Simple text editor with save/load functionality
*   Multi-process support with foreground/background switching
*   Event-driven I/O with per-process event queues
*   System calls interface

### Process Management
*   Cooperative and preemptive multitasking
*   Lottery-based scheduling algorithm with configurable tickets
*   Per-process virtual memory with page directory isolation
*   Event-driven process synchronization using hooks
*   Process states: running, yielding, waiting for events
*   I/O event queue system for keyboard and other input
*   Foreground process management for keyboard focus
*   System calls: yield, start_process, exit, poll/wait for I/O events

## Getting Started

### Prerequisites

To build and run ContinuumOS, you will need the following tools:

*   An `i686-elf` cross-compiler toolchain (GCC, G++, AS)
*   `make`
*   `qemu-system-i386`
*   `grub` (for creating the bootable ISO)

### Building

To build the kernel, run the following command:

```sh
make all
```

This will create the kernel binary at `kernel/kernel.bin`.

### Running

To run the OS in QEMU with FAT32 disk support:

```sh
make run
```

To run without any attached disks:

```sh
make run-nodisk
```

To create a bootable ISO image and run it in QEMU:

```sh
make runiso
```

The ISO image will be created as `kernel.iso`.

### Cleaning

To remove all build artifacts:

```sh
make clean
```

To remove all artifacts including test disk images:

```sh
make clean-all
```

### Test Disk Image

To recreate the test FAT32 disk image:

```sh
make test_fat32.img
```

This creates a 16MB FAT32 disk with a test file `FAT32README` by copying from the pre-built template `fat32_template.img`.

Note: To rebuild the FAT32 template from scratch, you'll need `mtools` installed.

## Shell Commands

### File System Commands
- `ls [path]` - List directory contents (works with all mounted filesystems)
- `cd <dir>` - Change directory  
- `cat <file>` - Display file contents (works with all mounted filesystems)
- `touch <file>` - Create a new file
- `mkdir <dir>` - Create a directory
- `rm <file>` - Remove a file
- `rmdir <dir>` - Remove a directory
- `pwd` - Print working directory

### Filesystem Management
- `mount [type]` - Mount a filesystem (e.g., `mount fat32`)
- `umount <mountpoint>` - Unmount a filesystem
- `fsinfo` - Show mounted filesystem information

### System Commands
- `help` - Show available commands
- `echo <text>` - Print text
- `uptime` - Show system uptime
- `history` - Show command history
- `edit <file>` - Edit a file (use `.save` to save, `.exit` to quit)
- `ps` - List running processes (if implemented)
- `meminfo` - Display detailed memory usage information (physical memory, heap statistics, memory layout)
- `free` - Display memory usage summary in a Linux-style format

### Hardware Commands
- `lsblk` - List block devices
- `disktest` - Test disk reading functionality

## Architecture

### Process Management
ContinuumOS implements a cooperative and preemptive multitasking system with the following features:

**Scheduling**: Uses a lottery-based scheduling algorithm where each process has a configurable number of tickets. Processes with more tickets have a higher probability of being selected to run.

**Process Structure**: Each process maintains:
- CPU context (registers, stack pointer, instruction pointer)
- Isolated page directory for virtual memory
- Per-process kernel stack
- Event queue for I/O operations
- Hook system for event-driven synchronization
- Keyboard handler callback

**Event-Driven Synchronization**: Processes can register hooks to wait for specific events:
- `HOOK_TIMER`: Wait for a specific logical time
- `HOOK_KEYBOARD`: Wait for keyboard input
- `HOOK_IO`: Wait for I/O operations

**System Calls**: User processes can interact with the kernel via software interrupts (int 0x80):
- `syscall_yield()`: Voluntarily yield CPU to another process
- `syscall_yield_for_event()`: Yield and wait for a specific event
- `syscall_start_process()`: Create and start a new process
- `syscall_exit()`: Terminate the current process
- `syscall_poll_io_event()`: Check for I/O events without blocking
- `syscall_wait_io_event()`: Wait for an I/O event (blocking)

**Foreground Process**: The scheduler maintains a foreground process concept for keyboard input. Only the foreground process receives keyboard events directly.

## Project Structure

```
.
├── boot/           # Bootloader assembly code
├── build/          # Build artifacts 
├── include/        # Header files for the kernel and libc
│   └── kernel/     # Kernel subsystem headers
│       ├── process.h      # Process management structures
│       ├── scheduler.h    # Scheduler interface
│       ├── hooks.h        # Event hooks system
│       └── ...
├── kernel/         # Kernel binaries 
├── libc/           # A basic C library implementation
│   ├── include/
│   │   └── sys/
│   │       ├── syscall.h  # System call interface
│   │       └── events.h   # I/O event structures
│   ├── process.c          # Process management helpers
│   └── ...
├── src/            # Source code for the kernel and utilities
│   ├── kernel/     # Kernel implementation
│   │   ├── process.cpp    # Process management
│   │   ├── scheduler.cpp  # Process scheduler
│   │   ├── syscalls.cpp   # System call handlers
│   │   └── ...
│   ├── bin/        # User programs
│   │   └── test_proc.cpp  # Test process
│   └── user/       # User-space applications
│       └── editor.cpp     # Text editor process
├── test_fat32.img  # Test FAT32 disk image
├── grub.cfg        # GRUB configuration
├── linker.ld       # Linker script
├── Makefile        # Build script
└── README.md       # This file
```