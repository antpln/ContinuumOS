# ContinuumOS

ContinuumOS is a light-weight minimal x86 kernel for training purposes. The
build system can target other architectures by setting the `ARCH` variable.
An experimental port for the RISC-V RV64GC ISA is included.

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

*   A cross-compiler toolchain for your target architecture (defaults to `i686-elf`)
*   `make`
*   A QEMU binary for your target (e.g. `qemu-system-i386`)
*   `grub` (for creating the bootable ISO)

### Building

To build the kernel, run the following command:

```sh
make all
```

You can override the `ARCH` variable to build for another target, for example:

```sh
make ARCH=x86_64

# Build the experimental RISC-V port
make ARCH=riscv64
```

This will create the kernel binary at `kernel/kernel.bin`.

To build every supported port sequentially, run:

```sh
make check-ports
```

### Running

To run the OS in QEMU:

```sh
make run
```

For RISC-V:

```sh
make ARCH=riscv64 run
```

To create a bootable ISO image and run it in QEMU:

```sh
make runiso
```

The ISO image will be created as `kernel.iso` (x86 only).

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