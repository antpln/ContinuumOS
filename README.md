# ContinuumOS

ContinuumOS is a light-weight minimal i686 kernel for training purposes.

## Features

*   32-bit Kernel
*   VGA Text Mode Driver
*   Keyboard Driver
*   Interrupt Descriptor Table (IDT) and Interrupt Service Routines (ISR)
*   Paging and Memory Management
*   Kernel Heap
*   Programmable Interrupt Controller (PIC)
*   Programmable Interval Timer (PIT)
*   A basic interactive shell
*   A simple text editor
*   System Calls
*   RamFS (simple RAM-based filesystem)

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

To run the OS in QEMU:

```sh
make run
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

## Project Structure

```
.
├── boot/           # Bootloader assembly code
├── build/          # Build artifacts 
├── include/        # Header files for the kernel and libc
├── kernel/         # Kernel binaries 
├── libc/           # A basic C library implementation
├── src/            # Source code for the kernel and utilities
├── .gitignore      # Git ignore file
├── grub.cfg        # GRUB configuration
├── linker.ld       # Linker script
├── Makefile        # Build script
└── README.md       # This file
```