# Minimal Makefile for building the Rust kernel

TARGET := i686-unknown-linux-gnu
RUST_LIB := rust_kernel/target/$(TARGET)/debug/librust_kernel.a
BOOT_OBJ := build/boot.o
KERNEL_ELF := kernel/kernel.bin

CC := gcc -m32
AS := as --32

.PHONY: all rustkernel iso run runiso clean

all: $(KERNEL_ELF)

$(BOOT_OBJ): boot/boot.s
	mkdir -p $(dir $@)
	$(AS) $< -o $@
rustkernel:
	cargo +nightly build \
	-Z build-std=core,compiler_builtins \
	-Z build-std-features=compiler-builtins-mem \
	--manifest-path rust_kernel/Cargo.toml \
	--target $(TARGET)

$(KERNEL_ELF): $(BOOT_OBJ) rustkernel linker.ld
	mkdir -p $(dir $@)
	$(CC) -T linker.ld -nostdlib -o $@ $(BOOT_OBJ) $(RUST_LIB) -lgcc
	grub-file --is-x86-multiboot $@

iso: $(KERNEL_ELF)
	mkdir -p isodir/boot/grub
	cp $(KERNEL_ELF) isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso isodir

run: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF)

runiso: iso
	qemu-system-i386 -cdrom kernel.iso

clean:
	rm -rf build kernel.iso isodir kernel
	rm -rf rust_kernel/target
