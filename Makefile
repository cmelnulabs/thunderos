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

.PHONY: all clean run debug fs userland test test-quick help

# Default target - must be first
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

# Help target - show available targets
help:
	@echo ""
	@echo "$(BOLD)$(CYAN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(CYAN)  ThunderOS Build System$(RESET)"
	@echo "$(BOLD)$(CYAN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""
	@echo "$(BOLD)Build Targets:$(RESET)"
	@echo "  $(GREEN)make$(RESET)              Build kernel (ELF and binary)"
	@echo "  $(GREEN)make all$(RESET)          Same as 'make'"
	@echo "  $(GREEN)make clean$(RESET)        Remove all build artifacts"
	@echo "  $(GREEN)make userland$(RESET)     Build userland programs only"
	@echo "  $(GREEN)make fs$(RESET)           Build ext2 filesystem image"
	@echo ""
	@echo "$(BOLD)Run Targets:$(RESET)"
	@echo "  $(GREEN)make run$(RESET)          Build and run in QEMU (text mode)"
	@echo "  $(GREEN)make qemu$(RESET)         Same as 'make run'"
	@echo "  $(GREEN)make qemu-gpu$(RESET)     Run with VirtIO GPU (VNC on :5900)"
	@echo "  $(GREEN)make qemu-gpu-web$(RESET) Run with GPU + noVNC (http://localhost:6080)"
	@echo ""
	@echo "$(BOLD)Debug Targets:$(RESET)"
	@echo "  $(GREEN)make debug$(RESET)        Run QEMU with GDB server (port 1234)"
	@echo "  $(GREEN)make gdb$(RESET)          Connect GDB client to debug session"
	@echo "  $(GREEN)make qemu-debug-mem$(RESET) Run with memory/MMU logging"
	@echo "  $(GREEN)make dump$(RESET)         Generate disassembly listing"
	@echo ""
	@echo "$(BOLD)Test Targets:$(RESET)"
	@echo "  $(GREEN)make test$(RESET)         Run full test suite"
	@echo "  $(GREEN)make test-quick$(RESET)   Run quick tests only"
	@echo ""
	@echo "$(BOLD)Build Options:$(RESET)"
	@echo "  $(YELLOW)ENABLE_TESTS=1$(RESET)    Include kernel tests in build"
	@echo "  $(YELLOW)TEST_MODE=1$(RESET)       Run tests and halt (no shell)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  $(CYAN)make run$(RESET)                    # Quick start"
	@echo "  $(CYAN)make qemu-gpu-web$(RESET)           # Run with graphics"
	@echo "  $(CYAN)make ENABLE_TESTS=1 test$(RESET)    # Run with kernel tests"
	@echo ""
	@echo "$(BOLD)$(CYAN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
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

# Create ext2 filesystem image (always rebuild to pick up Makefile changes)
fs: force_fs

force_fs: userland
	@echo ""
	@echo "$(BOLD)$(MAGENTA)[FS]$(RESET) Creating ext2 filesystem ($(FS_SIZE))..."
	@rm -rf $(BUILD_DIR)/testfs
	@rm -f $(FS_IMG)
	@mkdir -p $(BUILD_DIR)/testfs/bin
	@echo "Hello from ThunderOS ext2 filesystem!" > $(BUILD_DIR)/testfs/test.txt
	@echo "This is a sample file for testing." > $(BUILD_DIR)/testfs/README.txt
	@# Create test files for rm/rmdir testing
	@echo "Delete me with rm command!" > $(BUILD_DIR)/testfs/deleteme.txt
	@echo "Another file to delete" > $(BUILD_DIR)/testfs/removable.txt
	@mkdir -p $(BUILD_DIR)/testfs/emptydir
	@mkdir -p $(BUILD_DIR)/testfs/nonemptydir
	@echo "This file makes the directory non-empty" > $(BUILD_DIR)/testfs/nonemptydir/nested.txt
	@# Create startup script
	@echo "# ThunderOS Startup Script" > $(BUILD_DIR)/testfs/startup.sh
	@echo "echo ================================" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "echo   Welcome to ThunderOS!" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "echo ================================" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "export PATH=/bin" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "export HOME=/" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "export USER=root" >> $(BUILD_DIR)/testfs/startup.sh
	@echo "echo System ready." >> $(BUILD_DIR)/testfs/startup.sh
	@# Create demo script
	@echo "# ThunderOS Demo Script" > $(BUILD_DIR)/testfs/demo.sh
	@echo "echo === System Demo ===" >> $(BUILD_DIR)/testfs/demo.sh
	@echo "ls /" >> $(BUILD_DIR)/testfs/demo.sh
	@echo "pwd" >> $(BUILD_DIR)/testfs/demo.sh
	@echo "echo === Demo Complete ===" >> $(BUILD_DIR)/testfs/demo.sh
	@cp userland/build/cat $(BUILD_DIR)/testfs/bin/cat 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) cat not built"
	@cp userland/build/ls $(BUILD_DIR)/testfs/bin/ls 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) ls not built"
	@cp userland/build/hello $(BUILD_DIR)/testfs/bin/hello 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) hello not built"
	@cp userland/build/clock $(BUILD_DIR)/testfs/bin/clock 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) clock not built"
	@cp userland/build/mkdir $(BUILD_DIR)/testfs/bin/mkdir 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) mkdir not built"
	@cp userland/build/rmdir $(BUILD_DIR)/testfs/bin/rmdir 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) rmdir not built"
	@cp userland/build/pwd $(BUILD_DIR)/testfs/bin/pwd 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pwd not built"
	@cp userland/build/touch $(BUILD_DIR)/testfs/bin/touch 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) touch not built"
	@cp userland/build/rm $(BUILD_DIR)/testfs/bin/rm 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) rm not built"
	@cp userland/build/clear $(BUILD_DIR)/testfs/bin/clear 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) clear not built"
	@cp userland/build/sleep $(BUILD_DIR)/testfs/bin/sleep 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) sleep not built"
	@cp userland/build/chmod $(BUILD_DIR)/testfs/bin/chmod 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) chmod not built"
	@cp userland/build/chown $(BUILD_DIR)/testfs/bin/chown 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) chown not built"
	@cp userland/build/ush $(BUILD_DIR)/testfs/bin/ush 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) ush not built"
	@cp userland/build/ps $(BUILD_DIR)/testfs/bin/ps 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) ps not built"
	@cp userland/build/uname $(BUILD_DIR)/testfs/bin/uname 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) uname not built"
	@cp userland/build/uptime $(BUILD_DIR)/testfs/bin/uptime 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) uptime not built"
	@cp userland/build/whoami $(BUILD_DIR)/testfs/bin/whoami 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) whoami not built"
	@cp userland/build/tty $(BUILD_DIR)/testfs/bin/tty 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) tty not built"
	@cp userland/build/kill $(BUILD_DIR)/testfs/bin/kill 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) kill not built"
	@cp userland/build/signal_test $(BUILD_DIR)/testfs/bin/signal_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) signal_test not built"
	@cp userland/build/pipe_test $(BUILD_DIR)/testfs/bin/pipe_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pipe_test not built"
	@cp userland/build/pipe_simple_test $(BUILD_DIR)/testfs/bin/pipe_simple_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) pipe_simple_test not built"
	@cp userland/build/mutex_test $(BUILD_DIR)/testfs/bin/mutex_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) mutex_test not built"
	@cp userland/build/condvar_test $(BUILD_DIR)/testfs/bin/condvar_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) condvar_test not built"
	@cp userland/build/rwlock_test $(BUILD_DIR)/testfs/bin/rwlock_test 2>/dev/null || echo "  $(YELLOW)Warning:$(RESET) rwlock_test not built"
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

# Run with GPU support (opens graphical window)
qemu-gpu: userland fs
	@rm -f $(BUILD_DIR)/kernel/main.o
	@$(MAKE) --no-print-directory TEST_MODE=0 all
	@echo ""
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(MAGENTA)  Starting ThunderOS in QEMU (with GPU)$(RESET)"
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)VNC Display:$(RESET) Connect to localhost:5900 from host"
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@if command -v qemu-system-riscv64 >/dev/null 2>&1; then \
		qemu-system-riscv64 -machine virt -m 128M \
			-serial mon:stdio \
			-bios none \
			-kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-device virtio-gpu-device \
			-vnc :0; \
	elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then \
		/tmp/qemu-10.1.2/build/qemu-system-riscv64 -machine virt -m 128M \
			-serial mon:stdio \
			-bios none \
			-kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-device virtio-gpu-device \
			-vnc :0; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) qemu-system-riscv64 not found"; \
		exit 1; \
	fi

# Run with GPU and web-based VNC viewer (no VNC client needed)
qemu-gpu-web: userland fs
	@rm -f $(BUILD_DIR)/kernel/main.o
	@$(MAKE) --no-print-directory TEST_MODE=0 all
	@echo ""
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(MAGENTA)  Starting ThunderOS in QEMU (with Web VNC)$(RESET)"
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)Open in browser:$(RESET) http://localhost:6080/vnc.html"
	@echo "$(BOLD)$(MAGENTA)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""
	@# Start websockify in background to bridge web to VNC
	@websockify --web=/usr/share/novnc 6080 localhost:5900 &
	@sleep 1
	@if command -v qemu-system-riscv64 >/dev/null 2>&1; then \
		qemu-system-riscv64 -machine virt -m 128M \
			-serial mon:stdio \
			-bios none \
			-kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-device virtio-gpu-device \
			-vnc :0; \
	elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then \
		/tmp/qemu-10.1.2/build/qemu-system-riscv64 -machine virt -m 128M \
			-serial mon:stdio \
			-bios none \
			-kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-device virtio-gpu-device \
			-vnc :0; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) qemu-system-riscv64 not found"; \
		exit 1; \
	fi
	@# Clean up websockify
	@pkill -f "websockify.*6080" 2>/dev/null || true

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

# Memory debugging with QEMU tracing
.PHONY: qemu-debug-mem
qemu-debug-mem: $(KERNEL_ELF) $(BUILD_DIR)/fs.img
	@echo ""
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(YELLOW)  Memory Debug Mode$(RESET)"
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)Logging:$(RESET) guest errors, interrupts only"
	@echo "  $(CYAN)Output:$(RESET)  memory_debug.log"
	@echo "  $(CYAN)Tip:$(RESET)     Add -d mmu for verbose MMU logging (large files!)"
	@echo "$(BOLD)$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""
	@if command -v qemu-system-riscv64 >/dev/null 2>&1; then \
		qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-d guest_errors,int -D memory_debug.log; \
	elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then \
		/tmp/qemu-10.1.2/build/qemu-system-riscv64 $(QEMU_FLAGS) -kernel $(KERNEL_ELF) \
			-global virtio-mmio.force-legacy=false \
			-drive file=$(FS_IMG),if=none,format=raw,id=hd0 \
			-device virtio-blk-device,drive=hd0 \
			-d guest_errors,int -D memory_debug.log; \
	else \
		echo "$(RED)✗ ERROR:$(RESET) qemu-system-riscv64 not found"; \
		exit 1; \
	fi

# GDB client helper
.PHONY: gdb
gdb: $(KERNEL_ELF)
	@echo ""
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "$(BOLD)$(GREEN)  Connecting to GDB Server$(RESET)"
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo "  $(CYAN)Target:$(RESET) localhost:1234"
	@echo "  $(CYAN)Kernel:$(RESET) $(KERNEL_ELF)"
	@echo "$(BOLD)$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(RESET)"
	@echo ""
	riscv64-unknown-elf-gdb $(KERNEL_ELF) \
		-ex "target remote :1234" \
		-ex "layout split"

# Quick run: build everything and run QEMU with shell
run: qemu
