#!/bin/bash
# Compile user test program
# This produces a standalone user-mode program

set -euo pipefail

readonly CROSS_COMPILE="${CROSS_COMPILE:-riscv64-unknown-elf-}"
readonly CC="${CROSS_COMPILE}gcc"
readonly LD="${CROSS_COMPILE}ld"
readonly OBJCOPY="${CROSS_COMPILE}objcopy"

# User test entry point address
readonly USER_TEST_ENTRY="0x10000"

# Compile user test
echo "Compiling user test program..."
"${CC}" -march=rv64gc -mabi=lp64d -mcmodel=medany \
    -nostdlib -nostartfiles -ffreestanding -fno-common \
    -O0 -Wall -Wextra \
    -Iinclude \
    -c tests/user_test.c -o build/user_test.o

# Link user test (no special linker script, load at USER_TEST_ENTRY)
echo "Linking user test program..."
"${LD}" -m elf64lriscv -Ttext="${USER_TEST_ENTRY}" \
    build/user_test.o -o build/user_test.elf

# Convert to binary
echo "Creating user test binary..."
"${OBJCOPY}" -O binary build/user_test.elf build/user_test.bin

echo "User test program built: build/user_test.bin"
echo "Size: $(wc -c < build/user_test.bin) bytes"
