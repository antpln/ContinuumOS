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

### Hardware Support
*   Programmable Interrupt Controller (PIC)
*   Programmable Interval Timer (PIT)
*   IDE/ATA Hard Disk Driver (PIO mode)
*   Block Device Abstraction Layer

### Filesystems
*   RamFS (simple RAM-based filesystem)
*   FAT32 Filesystem (read-only support)
*   File operations: open, read, seek, close
*   Directory listing and navigation

### User Interface
*   Interactive shell with command history
*   Simple text editor with save/load functionality
*   System calls interface

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

### File System Commands (RAMFS)
- `ls` - List directory contents
- `cd <dir>` - Change directory  
- `cat <file>` - Display file contents
- `touch <file>` - Create a new file
- `mkdir <dir>` - Create a directory
- `rm <file>` - Remove a file
- `rmdir <dir>` - Remove a directory
- `pwd` - Print working directory

### FAT32 Commands
- `mount` - Mount FAT32 filesystem from attached disk
- `fsinfo` - Show mounted filesystem information
- `fat32ls` - List FAT32 root directory
- `fat32cat <file>` - Read and display FAT32 file contents

### System Commands
- `help` - Show available commands
- `echo <text>` - Print text
- `uptime` - Show system uptime
- `history` - Show command history
- `edit <file>` - Edit a file (use `.save` to save, `.exit` to quit)

### Hardware Commands
- `lsblk` - List block devices
- `disktest` - Test disk reading functionality

## Project Structure

```
.
├── boot/           # Bootloader assembly code
├── build/          # Build artifacts 
├── include/        # Header files for the kernel and libc
│   └── kernel/     # Kernel subsystem headers
├── kernel/         # Kernel binaries 
├── libc/           # A basic C library implementation
├── src/            # Source code for the kernel and utilities
│   └── kernel/     # Kernel implementation
├── test_fat32.img  # Test FAT32 disk image
├── grub.cfg        # GRUB configuration
├── linker.ld       # Linker script
├── Makefile        # Build script
└── README.md       # This file
```