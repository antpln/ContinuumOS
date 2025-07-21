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
*   Kernel Tests (memory, paging and heap)

## Getting Started

### Prerequisites

To build and run ContinuumOS, you will need the following tools:

*   An `i686-elf` cross-compiler toolchain (GCC, G++, AS)
*   `make`
*   `qemu-system-i386`
*   `grub` (for creating the bootable ISO)
*   A recent Rust toolchain (via [rustup](https://rustup.rs))

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

### Experimental Rust Port

An initial Rust-based kernel is provided in `rust_kernel`. Build the static
library with:

```sh
make rustkernel
```

This compiles the Rust code for the `i686-unknown-uefi` target using Cargo.
Before building, install the Rust toolchain and add the target with:

```sh
rustup toolchain install stable
rustup target add i686-unknown-uefi
```

The port currently includes VGA terminal, I/O port, PIC, GDT, IDT, basic ISR handling, PIT timer, a simple physical memory manager, early paging support, a basic heap allocator, a rudimentary RAMFS, system call stubs, a keyboard driver, kernel self-tests and a minimal shell written in Rust.
## Project Structure

```
.
├── boot/           # Bootloader assembly code
├── build/          # Build artifacts
├── include/        # Header files for the kernel and libc
├── kernel/         # Kernel binaries
├── libc/           # A basic C library implementation
├── src/            # Source code for the kernel and utilities
├── rust_kernel/    # Experimental Rust port
├── .gitignore      # Git ignore file
├── grub.cfg        # GRUB configuration
├── linker.ld       # Linker script
├── Makefile        # Build script
└── README.md       # This file
```