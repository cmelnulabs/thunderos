# ThunderOS Makefile

# Color output
BOLD := \033[1m
RESET := \033[0m
GREEN := \033[32m
BLUE := \033[34m
YELLOW := \033[33m
RED := \033[31m
CYAN := \033[36m
MAGENTA := \033[35m

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
ENABLE_TESTS ?= 0
TEST_MODE ?= 0

# Compiler flags
CFLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany
CFLAGS += -nostdlib -nostartfiles -ffreestanding -fno-common
CFLAGS += -O0 -g -Wall -Wextra
CFLAGS += -I$(INCLUDE_DIR)

# Enable kernel tests (set ENABLE_TESTS=0 to disable)
ifeq ($(ENABLE_TESTS),1)
    CFLAGS += -DENABLE_KERNEL_TESTS
endif

# Test mode: run tests and halt without launching shell
ifeq ($(TEST_MODE),1)
    CFLAGS += -DTEST_MODE
endif

# Linker flags
LDFLAGS := -nostdlib -T kernel/arch/riscv64/kernel.ld

# Source files
BOOT_SOURCES := $(wildcard $(BOOT_DIR)/*.S) $(wildcard $(BOOT_DIR)/*.c)
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
                        tests/unit/test_elf.c \
                        tests/unit/test_memory_isolation.c
endif

KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_DIR)/arch/riscv64/*.S)

# Test programs (no longer used)
TEST_ASM_SOURCES :=
TEST_ASM_OBJS :=

KERNEL_ASM_SOURCES := $(sort $(KERNEL_ASM_SOURCES))

# Object files
BOOT_OBJS := $(patsubst $(BOOT_DIR)/%.S,$(BUILD_DIR)/boot/%.o,$(filter %.S,$(BOOT_SOURCES))) \
             $(patsubst $(BOOT_DIR)/%.c,$(BUILD_DIR)/boot/%.o,$(filter %.c,$(BOOT_SOURCES)))
KERNEL_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

# Remove duplicates
ALL_OBJS := $(sort $(BOOT_OBJS) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS) $(TEST_ASM_OBJS))

# Target binary
KERNEL_ELF := $(BUILD_DIR)/thunderos.elf
KERNEL_BIN := $(BUILD_DIR)/thunderos.bin

# QEMU flags for -bios none (run our own M-mode code, not OpenSBI)
QEMU_FLAGS := -machine virt -m 128M -nographic -serial mon:stdio
QEMU_FLAGS += -bios none

# Filesystem image
FS_IMG := $(BUILD_DIR)/fs.img
FS_SIZE := 10M

.PHONY: all clean run debug fs userland test test-quick

all: $(KERNEL_ELF) $(KERNEL_BIN)
	@echo ""
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(GREEN)  ThunderOS Build Complete!$(RESET)"
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)ELF:$(RESET) $(KERNEL_ELF)"
	@echo "  $(CYAN)BIN:$(RESET) $(KERNEL_BIN)"
	@echo ""
	@echo "  $(YELLOW)Run with:$(RESET) make qemu"
	@echo "  $(YELLOW)Debug:$(RESET)    make debug"
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""

$(KERNEL_ELF): $(ALL_OBJS)
	@echo "$(BOLD)$(BLUE)[LINK]$(RESET) $@"
	@mkdir -p $(dir $@)
	@$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "$(BOLD)$(BLUE)[BIN]$(RESET)  $@"
	@$(OBJCOPY) -O binary $< $@

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@echo "$(BOLD)$(CYAN)[CC]$(RESET)   $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile test assembly programs
$(BUILD_DIR)/tests/%.o: tests/%.S
	@echo "$(BOLD)$(CYAN)[AS]$(RESET)   $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly sources
$(BUILD_DIR)/%.o: %.S
	@echo "$(BOLD)$(CYAN)[AS]$(RESET)   $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "$(BOLD)$(RED)[CLEAN]$(RESET) Removing build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)✓ Clean complete$(RESET)"

# Create ext2 filesystem image
fs: $(FS_IMG)

$(FS_IMG): userland
	@echo ""
	@echo "$(BOLD)$(MAGENTA)[FS]$(RESET) Creating ext2 filesystem ($(FS_SIZE))..."
	@rm -rf $(BUILD_DIR)/testfs
	@mkdir -p $(BUILD_DIR)/testfs/bin
	@echo "Hello from ThunderOS ext2 filesystem!" > $(BUILD_DIR)/testfs/test.txt
	@echo "This is a sample file for testing." > $(BUILD_DIR)/testfs/README.txt
	@cp userland/build/cat $(BUILD_DIR)/testfs/bin/cat 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) cat not built"
	@cp userland/build/ls $(BUILD_DIR)/testfs/bin/ls 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) ls not built"
	@cp userland/build/hello $(BUILD_DIR)/testfs/bin/hello 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) hello not built"
	@cp userland/build/mkdir $(BUILD_DIR)/testfs/bin/mkdir 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) mkdir not built"
	@cp userland/build/rmdir $(BUILD_DIR)/testfs/bin/rmdir 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) rmdir not built"
	@cp userland/build/pwd $(BUILD_DIR)/testfs/bin/pwd 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pwd not built"
	@cp userland/build/touch $(BUILD_DIR)/testfs/bin/touch 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) touch not built"
	@cp userland/build/rm $(BUILD_DIR)/testfs/bin/rm 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) rm not built"
	@cp userland/build/clear $(BUILD_DIR)/testfs/bin/clear 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) clear not built"
	@cp userland/build/sleep $(BUILD_DIR)/testfs/bin/sleep 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) sleep not built"
	@cp userland/build/ush $(BUILD_DIR)/testfs/bin/ush 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) ush not built"
	@cp userland/build/signal_test $(BUILD_DIR)/testfs/bin/signal_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) signal_test not built"
	@cp userland/build/pipe_test $(BUILD_DIR)/testfs/bin/pipe_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pipe_test not built"
	@cp userland/build/pipe_simple_test $(BUILD_DIR)/testfs/bin/pipe_simple_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pipe_simple_test not built"
	@if command -v mkfs.ext2 >/dev/null 2>&1; then \
		mkfs.ext2 -F -q -d $(BUILD_DIR)/testfs $(FS_IMG) $(FS_SIZE) 2>&1 | grep -v "^mke2fs" | grep -v "^Creating" | grep -v "^Allocating" | grep -v "^Writing" | grep -v "^Copying" || true; \
		rm -rf $(BUILD_DIR)/testfs; \
		echo "$(GREEN)✓ Filesystem created:$(RESET) $(FS_IMG)"; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) mkfs.ext2 not found. Install e2fsprogs"; \
		exit 1; \
	fi
	@echo ""

userland:
	@echo ""
	@echo "$(BOLD)$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(BLUE)  Building Userland Programs$(RESET)"
	@echo "$(BOLD)$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@chmod +x build_userland.sh
	@./build_userland.sh
	@echo "$(BOLD)$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""

test:
	@cd tests/scripts && bash test_runner.sh

test-quick:
	@cd tests/scripts && bash test_runner.sh --quick

qemu: userland fs
	@rm -f $(BUILD_DIR)/kernel/main.o
	@$(MAKE) --no-print-directory TEST_MODE=0 all
	@echo ""
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(GREEN)  Starting ThunderOS in QEMU$(RESET)"
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@if command -v qemu-system-riscv64 >/dev/null 2>&1; then \
		QEMU_VERSION=$$(qemu-system-riscv64 --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1); \
		QEMU_MAJOR=$$(echo $$QEMU_VERSION | cut -d. -f1); \
		echo "  $(CYAN)QEMU Version:$(RESET) $$QEMU_VERSION"; \
		echo "  $(CYAN)Machine:$(RESET)      virt (128M RAM)"; \
		echo "  $(CYAN)Filesystem:$(RESET)   $(FS_IMG)"; \
		echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"; \
		echo ""; \
		if [ "$$QEMU_MAJOR" -lt 10 ]; then \
			echo "$(RED)✗ WARNING:$(RESET) QEMU $$QEMU_VERSION detected. ThunderOS requires QEMU 10.1.2+"; \
			echo "$(YELLOW)Please build and run in Docker, or install QEMU 10.1.2+$(RESET)"; \
			exit 1; \
		fi; \
		qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0; \
	elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then \
		echo "  $(CYAN)QEMU:$(RESET)         /tmp/qemu-10.1.2/build/qemu-system-riscv64"; \
		echo "  $(CYAN)Machine:$(RESET)      virt (128M RAM)"; \
		echo "  $(CYAN)Filesystem:$(RESET)   $(FS_IMG)"; \
		echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"; \
		echo ""; \
		/tmp/qemu-10.1.2/build/qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) qemu-system-riscv64 not found. Please install QEMU 10.1.2+"; \
		exit 1; \
	fi

debug: $(KERNEL_ELF)
	@echo ""
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(YELLOW)  Debug Mode - Waiting for GDB$(RESET)"
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)GDB Server:$(RESET) localhost:1234"
	@echo "  $(CYAN)Command:$(RESET)    riscv64-unknown-elf-gdb $(KERNEL_ELF)"
	@echo "  $(CYAN)Connect:$(RESET)    (gdb) target remote :1234"
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""
	@if command -v qemu-system-riscv64 >/dev/null 2>&1; then \
		qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) -s -S; \
	elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then \
		/tmp/qemu-10.1.2/build/qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) -s -S; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) qemu-system-riscv64 not found"; \
		exit 1; \
	fi

dump: $(KERNEL_ELF)
	@echo "$(BOLD)$(BLUE)[DUMP]$(RESET) Generating disassembly..."
	@$(OBJDUMP) -d $< > $(BUILD_DIR)/thunderos.dump
	@echo "$(GREEN)✓ Disassembly saved:$(RESET) $(BUILD_DIR)/thunderos.dump"

# Quick run: build everything and run QEMU with shell
run: qemu
