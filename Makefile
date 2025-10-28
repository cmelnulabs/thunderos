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

# Compiler flags
CFLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany
CFLAGS += -nostdlib -nostartfiles -ffreestanding -fno-common
CFLAGS += -O2 -Wall -Wextra
CFLAGS += -I$(INCLUDE_DIR)

# Linker flags
LDFLAGS := -nostdlib -T kernel/arch/riscv64/kernel.ld

# Source files
BOOT_SOURCES := $(wildcard $(BOOT_DIR)/*.S)
KERNEL_C_SOURCES := $(wildcard $(KERNEL_DIR)/*.c) \
                    $(wildcard $(KERNEL_DIR)/core/*.c) \
                    $(wildcard $(KERNEL_DIR)/drivers/*.c) \
                    $(wildcard $(KERNEL_DIR)/arch/riscv64/*.c) \
                    $(wildcard $(KERNEL_DIR)/arch/riscv64/drivers/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_DIR)/arch/riscv64/*.S)

KERNEL_ASM_SOURCES := $(sort $(KERNEL_ASM_SOURCES))

# Object files
BOOT_OBJS := $(patsubst $(BOOT_DIR)/%.S,$(BUILD_DIR)/boot/%.o,$(BOOT_SOURCES))
KERNEL_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

# Remove duplicates
ALL_OBJS := $(sort $(BOOT_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS))

# Target binary
KERNEL_ELF := $(BUILD_DIR)/thunderos.elf
KERNEL_BIN := $(BUILD_DIR)/thunderos.bin

# QEMU options
QEMU := qemu-system-riscv64
QEMU_FLAGS := -machine virt -m 128M -nographic -serial mon:stdio
QEMU_FLAGS += -bios default

.PHONY: all clean qemu debug

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

# Compile assembly sources
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

qemu: $(KERNEL_ELF)
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF)

debug: $(KERNEL_ELF)
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF) -s -S

dump: $(KERNEL_ELF)
	$(OBJDUMP) -d $< > $(BUILD_DIR)/thunderos.dump
	@echo "Disassembly saved to $(BUILD_DIR)/thunderos.dump"
