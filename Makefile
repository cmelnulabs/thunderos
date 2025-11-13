# ThunderOS Makefile

# Toolchain
CROSS_COMPILE ?= riscv64-unknown-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Directories
BUILD_DIR := build
KERNEL_DIR := kernel
BOOT_DIR := boot
INCLUDE_DIR := include

# Build configuration
ENABLE_TESTS ?= 1

# Compiler flags
CFLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany
CFLAGS += -nostdlib -nostartfiles -ffreestanding -fno-common
CFLAGS += -O0 -Wall -Wextra
CFLAGS += -I$(INCLUDE_DIR)

# Enable kernel tests (set ENABLE_TESTS=0 to disable)
ifeq ($(ENABLE_TESTS),1)
    CFLAGS += -DENABLE_KERNEL_TESTS
endif

# Linker flags
LDFLAGS := -nostdlib -T kernel/arch/riscv64/kernel.ld

# Source files
BOOT_SOURCES := $(wildcard $(BOOT_DIR)/*.S)
KERNEL_C_SOURCES := $(wildcard $(KERNEL_DIR)/*.c) \
                    $(wildcard $(KERNEL_DIR)/core/*.c) \
                    $(wildcard $(KERNEL_DIR)/utils/*.c) \
                    $(wildcard $(KERNEL_DIR)/drivers/*.c) \
                    $(wildcard $(KERNEL_DIR)/mm/*.c) \
                    $(wildcard $(KERNEL_DIR)/fs/*.c) \
                    $(wildcard $(KERNEL_DIR)/arch/riscv64/*.c) \
                    $(wildcard $(KERNEL_DIR)/arch/riscv64/core/*.c) \
                    $(wildcard $(KERNEL_DIR)/arch/riscv64/drivers/*.c)

# Add test sources if enabled
ifeq ($(ENABLE_TESTS),1)
    KERNEL_C_SOURCES += tests/unit/test_memory_mgmt.c \
                        tests/unit/test_elf.c
endif

KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_DIR)/arch/riscv64/*.S)

# Test programs (no longer used)
TEST_ASM_SOURCES :=
TEST_ASM_OBJS :=

KERNEL_ASM_SOURCES := $(sort $(KERNEL_ASM_SOURCES))

# Object files
BOOT_OBJS := $(patsubst $(BOOT_DIR)/%.S,$(BUILD_DIR)/boot/%.o,$(BOOT_SOURCES))
KERNEL_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

# Remove duplicates
ALL_OBJS := $(sort $(BOOT_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS) $(TEST_ASM_OBJS))

# Target binary
KERNEL_ELF := $(BUILD_DIR)/thunderos.elf
KERNEL_BIN := $(BUILD_DIR)/thunderos.bin

# QEMU options
QEMU := qemu-system-riscv64
QEMU_FLAGS := -machine virt -m 128M -nographic -serial mon:stdio
QEMU_FLAGS += -bios default

# Filesystem image
FS_IMG := $(BUILD_DIR)/fs.img
FS_SIZE := 10M

.PHONY: all clean qemu debug fs userland test

all: $(KERNEL_ELF) $(KERNEL_BIN)

$(KERNEL_ELF): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	@echo "Built: $@"

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Built: $@"

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test assembly programs
$(BUILD_DIR)/tests/%.o: tests/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly sources
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

# Create ext2 filesystem image
fs: $(FS_IMG)

$(FS_IMG): userland
	@echo "Creating ext2 filesystem image ($(FS_SIZE))..."
	@rm -rf $(BUILD_DIR)/testfs
	@mkdir -p $(BUILD_DIR)/testfs/bin
	@echo "Hello from ThunderOS ext2 filesystem!" > $(BUILD_DIR)/testfs/test.txt
	@echo "This is a sample file for testing." > $(BUILD_DIR)/testfs/README.txt
	@cp userland/build/cat $(BUILD_DIR)/testfs/bin/cat 2>/dev/null || echo "⚠ cat not built"
	@cp userland/build/ls $(BUILD_DIR)/testfs/bin/ls 2>/dev/null || echo "⚠ ls not built"
	@cp userland/build/hello $(BUILD_DIR)/testfs/bin/hello 2>/dev/null || echo "⚠ hello not built"
	@if command -v mkfs.ext2 >/dev/null 2>&1; then \
		mkfs.ext2 -F -q -d $(BUILD_DIR)/testfs $(FS_IMG) $(FS_SIZE); \
		rm -rf $(BUILD_DIR)/testfs; \
		echo "✓ Filesystem created: $(FS_IMG)"; \
	else \
		echo "ERROR: mkfs.ext2 not found. Install e2fsprogs: sudo apt-get install e2fsprogs"; \
		exit 1; \
	fi

userland:
	@echo "Building userland programs..."
	@chmod +x build_userland.sh
	@./build_userland.sh

test:
	@echo "Running ThunderOS test suite..."
	@cd tests/scripts && bash run_all_tests.sh

qemu: $(KERNEL_ELF) $(FS_IMG)
	@echo "Running ThunderOS with ext2 filesystem..."
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF) \
		-global virtio-mmio.force-legacy=false \
		-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0

debug: $(KERNEL_ELF)
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF) -s -S

dump: $(KERNEL_ELF)
	$(OBJDUMP) -d $< > $(BUILD_DIR)/thunderos.dump
	@echo "Disassembly saved to $(BUILD_DIR)/thunderos.dump"
