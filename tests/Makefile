# Test Makefile for ThunderOS

# Toolchain
CROSS_COMPILE ?= riscv64-unknown-elf-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# Directories
BUILD_DIR := ../build/tests
FRAMEWORK_DIR := framework
INCLUDE_DIR := ../include
KERNEL_DIR := ../kernel

# Compiler flags
CFLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany
CFLAGS += -nostdlib -nostartfiles -ffreestanding -fno-common
CFLAGS += -O2 -Wall -Wextra
CFLAGS += -I$(INCLUDE_DIR) -I$(FRAMEWORK_DIR)

# Linker flags
LDFLAGS := -nostdlib -T $(KERNEL_DIR)/arch/riscv64/kernel.ld

# Common object files needed by tests
BOOT_OBJ := $(BUILD_DIR)/boot.o
UART_OBJ := $(BUILD_DIR)/uart.o
CLINT_OBJ := $(BUILD_DIR)/clint.o
TRAP_OBJ := $(BUILD_DIR)/trap.o
TRAP_ENTRY_OBJ := $(BUILD_DIR)/trap_entry.o
KUNIT_OBJ := $(BUILD_DIR)/kunit.o

COMMON_OBJS := $(BOOT_OBJ) $(UART_OBJ) $(TRAP_OBJ) $(TRAP_ENTRY_OBJ) $(KUNIT_OBJ)
COMMON_OBJS_WITH_CLINT := $(COMMON_OBJS) $(CLINT_OBJ)

# Test targets
TEST_TRAP := $(BUILD_DIR)/test_trap.elf
TEST_TIMER := $(BUILD_DIR)/test_timer.elf

.PHONY: all clean run-test-trap run-test-timer

all: $(TEST_TRAP) $(TEST_TIMER)

# Build test_trap
$(TEST_TRAP): $(BUILD_DIR)/test_trap.o $(COMMON_OBJS_WITH_CLINT)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

# Build test_timer
$(TEST_TIMER): $(BUILD_DIR)/test_timer.o $(COMMON_OBJS_WITH_CLINT)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"

# Compile test files
$(BUILD_DIR)/test_trap.o: test_trap.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_timer.o: test_timer.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile framework
$(KUNIT_OBJ): $(FRAMEWORK_DIR)/kunit.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile common kernel files
$(BOOT_OBJ): ../boot/boot.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(UART_OBJ): $(KERNEL_DIR)/drivers/uart.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(CLINT_OBJ): $(KERNEL_DIR)/drivers/clint.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TRAP_OBJ): $(KERNEL_DIR)/arch/riscv64/trap.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TRAP_ENTRY_OBJ): $(KERNEL_DIR)/arch/riscv64/trap_entry.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
run-test-trap: $(TEST_TRAP)
	@echo "Running trap handler tests..."
	@timeout 5 qemu-system-riscv64 -machine virt -m 128M -nographic \
		-serial mon:stdio -bios default -kernel $(TEST_TRAP) || true

run-test-timer: $(TEST_TIMER)
	@echo "Running timer interrupt tests..."
	@timeout 10 qemu-system-riscv64 -machine virt -m 128M -nographic \
		-serial mon:stdio -bios default -kernel $(TEST_TIMER) || true

clean:
	rm -rf $(BUILD_DIR)
