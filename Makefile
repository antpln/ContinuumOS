# Cross-compiler setup
CC = i686-elf-gcc
CXX = i686-elf-g++
AS = i686-elf-as

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = $(SRC_DIR)/kernel
KERNEL_DEST = ./kernel
LIBC_DIR = libc
USER_SRC_DIR = $(SRC_DIR)/user
APPS_DIR = apps

# Create kernel directory if it doesn't exist
$(shell mkdir -p $(KERNEL_DEST))

# Compiler flags
COMMON_FLAGS = -O2 -g -ffreestanding -Wall -Wextra -I$(INCLUDE_DIR) -I$(LIBC_DIR)/include -DDEBUG -DTEST
CFLAGS = $(COMMON_FLAGS) -std=gnu99
CFLAGS += $(EXTRA_CFLAGS)
CXXFLAGS = $(COMMON_FLAGS) -fno-exceptions -fno-rtti
CXXFLAGS += $(EXTRA_CFLAGS)
LDFLAGS = -ffreestanding -O2 -nostdlib

# Source files (exclude toolchain build directories)
CSOURCES = $(shell find $(SRC_DIR) -name '*.c' ! -path "*/binutils-*" ! -path "*/gcc-*" ! -path "*/build-*" ! -path "$(USER_SRC_DIR)/*")
CPPSOURCES = $(shell find $(SRC_DIR) -name '*.cpp' ! -path "*/binutils-*" ! -path "*/gcc-*" ! -path "*/build-*" ! -path "$(USER_SRC_DIR)/*")
ASMSOURCES = $(shell find $(BOOT_DIR) -name '*.s')
KERNEL_ASMSOURCES = $(shell find $(KERNEL_DIR) -name '*.s')
LIBC_CSOURCES = $(shell find $(LIBC_DIR) -type f -name '*.c')
LIBC_CPPSOURCES = $(shell find $(LIBC_DIR) -type f -name '*.cpp')
USER_SOURCES = $(wildcard $(USER_SRC_DIR)/*.cpp)

# Object files
KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.s,$(BUILD_DIR)/kernel/%.o,$(KERNEL_ASMSOURCES))
BOOT_OBJS = $(patsubst $(BOOT_DIR)/%.s,$(BUILD_DIR)/boot/%.o,$(ASMSOURCES))
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CSOURCES))
CPP_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPPSOURCES))
LIBC_C_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/libc/%,$(LIBC_CSOURCES:.c=.o))
LIBC_CPP_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/libc/%,$(LIBC_CPPSOURCES:.cpp=.o))
LIBC_USER_C_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/user_libc/%,$(LIBC_CSOURCES:.c=.o))
LIBC_USER_CPP_OBJS = $(patsubst $(LIBC_DIR)/%,$(BUILD_DIR)/user_libc/%,$(LIBC_CPPSOURCES:.cpp=.o))
USER_APP_OBJS = $(patsubst $(USER_SRC_DIR)/%.cpp,$(BUILD_DIR)/user_apps/%.o,$(USER_SOURCES))
USER_APP_MODULES = $(patsubst $(USER_SRC_DIR)/%.cpp,$(APPS_DIR)/%.app,$(USER_SOURCES))
APP_EMBED_OBJECTS = $(patsubst $(APPS_DIR)/%.app,$(BUILD_DIR)/app_bundles/%_app.o,$(USER_APP_MODULES))

OBJECTS = $(sort $(BOOT_OBJS) $(C_OBJS) $(CPP_OBJS) $(KERNEL_OBJS) $(LIBC_C_OBJS) $(LIBC_CPP_OBJS) $(APP_EMBED_OBJECTS))

# Output files
KERNEL_ELF = kernel/kernel.bin

# QEMU configuration
QEMU = qemu-system-i386
QEMU_FLAGS = -kernel $(KERNEL_ELF) -serial stdio

.PHONY: all clean clean-all resetimg run directories iso debug runiso release runrelease rundebug apps

all: directories $(KERNEL_ELF) apps

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/kernel
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/libc
	@mkdir -p $(BUILD_DIR)/libc/stdlib
	@mkdir -p $(BUILD_DIR)/user_libc
	@mkdir -p $(BUILD_DIR)/user_libc/stdlib
	@mkdir -p $(BUILD_DIR)/user_apps
	@mkdir -p $(BUILD_DIR)/app_bundles
	@mkdir -p $(APPS_DIR)

$(KERNEL_ELF): $(OBJECTS)
	$(CXX) -T linker.ld -o $(KERNEL_DEST)/kernel.bin $(LDFLAGS) $(OBJECTS) -lgcc
# grub-file --is-x86-multiboot $(KERNEL_DEST)/kernel.bin

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

# Update libc compilation rules
$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++: $<"
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling C: $<"
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR)/user_libc/%.o: $(LIBC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling user C++: $<"
	$(CXX) -c $< -o $@ $(CXXFLAGS) -DUSER_APP_BUILD

$(BUILD_DIR)/user_libc/%.o: $(LIBC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling user C: $<"
	$(CC) -c $< -o $@ $(CFLAGS) -DUSER_APP_BUILD

$(BUILD_DIR)/user_apps/%.o: $(USER_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -DUSER_APP_BUILD

$(APPS_DIR)/%.app: $(BUILD_DIR)/user_apps/%.o $(LIBC_USER_C_OBJS) $(LIBC_USER_CPP_OBJS)
	@mkdir -p $(dir $@)
	i686-elf-ld -r $^ -o $@

$(BUILD_DIR)/app_bundles/%_app.o: $(APPS_DIR)/%.app
	@mkdir -p $(dir $@)
	i686-elf-objcopy -I binary -O elf32-i386 -B i386 $< $@

apps: $(USER_APP_MODULES)

iso: $(KERNEL_ELF)
	mkdir -p isodir/boot/grub
	cp $(KERNEL_DEST)/kernel.bin isodir/boot/kernel.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	@if [ -f /usr/share/grub/unicode.pf2 ]; then \
		mkdir -p isodir/boot/grub/fonts; \
		cp /usr/share/grub/unicode.pf2 isodir/boot/grub/fonts/unicode.pf2; \
	elif [ -f /usr/share/grub/fonts/unicode.pf2 ]; then \
		mkdir -p isodir/boot/grub/fonts; \
		cp /usr/share/grub/fonts/unicode.pf2 isodir/boot/grub/fonts/unicode.pf2; \
	fi
	grub-mkrescue -o kernel.iso isodir

# Build ISO with debug/test logging disabled for release workflows
release:
	$(MAKE) clean
	$(MAKE) iso EXTRA_CFLAGS="-UDEBUG -UTEST"

rundebug :
	$(MAKE) clean
	$(MAKE) 
	$(QEMU) $(QEMU_FLAGS) -drive file=test_fat32.img,format=raw,if=ide

run: $(KERNEL_ELF) test_fat32.img
	$(QEMU) $(QEMU_FLAGS) -drive file=test_fat32.img,format=raw,if=ide

runrelease :
	$(MAKE) clean
	$(MAKE) EXTRA_CFLAGS="-UDEBUG -UTEST"
	$(QEMU) $(QEMU_FLAGS) -drive file=test_fat32.img,format=raw,if=ide

runiso: iso
	$(MAKE) iso EXTRA_CFLAGS="-UDEBUG -UTEST"
	$(QEMU) $(QEMU_FLAGS) -cdrom kernel.iso

clean:
	rm -rf $(BUILD_DIR)
	rm -rf isodir
	rm -f kernel.iso
	rm -f $(KERNEL_DEST)/kernel.bin
	rm -f $(KERNEL_DEST)/kernel2.bin
	rm -f $(APPS_DIR)/*.app

resetimg:
	rm -f test_fat32.img

clean-all: clean resetimg
	rm -f fat32_template.img

# Create test FAT32 disk image
test_fat32.img: fat32_template.img
	@echo "Creating test FAT32 disk image from template..."
	cp fat32_template.img test_fat32.img
	@echo "Test FAT32 disk image created successfully"

fat32_template.img:
	@echo "Template image missing; rebuilding..."
	./scripts/build_fat32_template.sh $@

# Add debug target
debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -O0 -g" ASFLAGS="--32"
	qemu-system-i386 -S -s -kernel kernel/kernel.bin &
