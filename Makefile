# Cross-compiler setup
# Architecture prefix (e.g. i686, x86_64, arm-none-eabi)
ARCH ?= i686
# List of available architecture ports that can be built.
ARCH_LIST ?= i686 riscv64
CROSS_PREFIX ?= $(ARCH)-elf-
CC  = $(CROSS_PREFIX)gcc
CXX = $(CROSS_PREFIX)g++
AS  = $(CROSS_PREFIX)as

# Architecture-specific compile flags
ARCH_CFLAGS :=
ARCH_CXXFLAGS :=
ifeq ($(ARCH),riscv64)
ARCH_CFLAGS   = -march=rv64gc -mabi=lp64
ARCH_CXXFLAGS = -march=rv64gc -mabi=lp64
endif

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = $(SRC_DIR)/kernel
KERNEL_DEST = ./kernel
LIBC_DIR = libc

# Create kernel directory if it doesn't exist
$(shell mkdir -p $(KERNEL_DEST))

# Compiler flags
CFLAGS = -O2 -g -std=gnu99 -ffreestanding -Wall -Wextra -I$(INCLUDE_DIR) -I$(LIBC_DIR)/include $(ARCH_CFLAGS)
CXXFLAGS = -O2 -g -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti -I$(INCLUDE_DIR) -I$(LIBC_DIR)/include $(ARCH_CXXFLAGS)
LDFLAGS = -ffreestanding -O2 -nostdlib

# Source files
ifeq ($(ARCH),riscv64)
CSOURCES = $(shell find src/riscv -name '*.c')
CPPSOURCES = $(shell find src/riscv -name '*.cpp')
ASMSOURCES = $(shell find boot/riscv -name '*.s')
KERNEL_ASMSOURCES =
LIBC_CSOURCES =
LIBC_CPPSOURCES =
else
CSOURCES = $(shell find $(SRC_DIR) -path $(SRC_DIR)/riscv -prune -o -name '*.c' -print)
CPPSOURCES = $(shell find $(SRC_DIR) -path $(SRC_DIR)/riscv -prune -o -name '*.cpp' -print)
ASMSOURCES = $(shell find $(BOOT_DIR) -path $(BOOT_DIR)/riscv -prune -o -name '*.s' -print)
KERNEL_ASMSOURCES = $(shell find $(KERNEL_DIR) -name '*.s')
LIBC_CSOURCES = $(shell find $(LIBC_DIR) -type f -name '*.c')
LIBC_CPPSOURCES = $(shell find $(LIBC_DIR) -type f -name '*.cpp')
endif

# Object files
KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.s,$(BUILD_DIR)/kernel/%.o,$(KERNEL_ASMSOURCES))
BOOT_OBJS = $(patsubst $(BOOT_DIR)/%.s,$(BUILD_DIR)/boot/%.o,$(ASMSOURCES))
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CSOURCES))
CPP_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPPSOURCES))
LIBC_C_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/libc/%,$(LIBC_CSOURCES:.c=.o))
LIBC_CPP_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/libc/%,$(LIBC_CPPSOURCES:.cpp=.o))

OBJECTS = $(sort $(BOOT_OBJS) $(C_OBJS) $(CPP_OBJS) $(KERNEL_OBJS) $(LIBC_C_OBJS) $(LIBC_CPP_OBJS))

# Output files
KERNEL_ELF = kernel/kernel.bin

# QEMU configuration
QEMU_ARCH ?= i386
ifeq ($(ARCH),riscv64)
QEMU_ARCH = riscv64
QEMU_FLAGS = -machine virt -bios none -kernel $(KERNEL_ELF)
else
QEMU_FLAGS = -kernel $(KERNEL_ELF)
endif
QEMU = qemu-system-$(QEMU_ARCH)

.PHONY: all clean run directories iso debug runiso check-ports

all: directories $(KERNEL_ELF)

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/kernel
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/libc

GRUB_CHECK :=
ifeq ($(filter $(ARCH),i686 x86_64),$(ARCH))
GRUB_CHECK := grub-file --is-x86-multiboot $(KERNEL_DEST)/kernel.bin
endif

$(KERNEL_ELF): $(OBJECTS)
	$(CXX) -T linker.ld -o $(KERNEL_DEST)/kernel.bin $(LDFLAGS) $(OBJECTS) -lgcc
	$(GRUB_CHECK)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR)/boot/%.o: $(BOOT_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $< -o $@

# Add .s files to the valid source extensions if not already present
SRCEXTS += .s

# Add assembly compilation rule
%.o: %.s
	$(AS) $< -o $@

# Update libc compilation rules
$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++: $<"
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling C: $<"
	$(CC) -c $< -o $@ $(CFLAGS)

ifeq ($(filter $(ARCH),i686 x86_64),$(ARCH))
iso: $(KERNEL_ELF)
	       mkdir -p isodir/boot/grub
	       cp $(KERNEL_DEST)/kernel.bin isodir/boot/kernel.bin
		       cp grub.cfg isodir/boot/grub/grub.cfg
		       $(CXX) -T linker.ld -o $(KERNEL_DEST)/kernel2.bin $(LDFLAGS) $(OBJECTS) -lgcc
	       cp $(KERNEL_DEST)/kernel2.bin isodir/boot/kernel.bin
	       grub-mkrescue -o kernel.iso isodir
else
iso:
	       @echo "ISO creation not supported for ARCH=$(ARCH)"
endif

run: $(KERNEL_ELF)
	$(QEMU) $(QEMU_FLAGS)

runiso: iso
	$(QEMU) -cdrom kernel.iso

clean:
	rm -rf $(BUILD_DIR)
	rm -rf isodir
	rm -f kernel.iso
	rm -f $(KERNEL_DEST)/kernel.bin

# Add debug target
debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O0 -g" ASFLAGS="--32"
	$(QEMU) -S -s $(QEMU_FLAGS) &

# Try building every architecture listed in ARCH_LIST
check-ports:
	@for arch in $(ARCH_LIST); do \
	echo "Building for $$arch"; \
	$(MAKE) clean > /dev/null; \
	if $(MAKE) ARCH=$$arch all > /dev/null; then \
	echo "[$$arch] build succeeded"; \
	else \
	echo "[$$arch] build failed"; exit 1; \
	fi; \
	done
