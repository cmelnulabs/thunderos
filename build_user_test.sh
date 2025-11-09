#!/bin/bash
# Compile user test program
# This produces a standalone user-mode program

set -e

CROSS_COMPILE=${CROSS_COMPILE:-riscv64-unknown-elf-}
CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"
OBJCOPY="${CROSS_COMPILE}objcopy"

# Compile user test
echo "Compiling user test program..."
$CC -march=rv64gc -mabi=lp64d -mcmodel=medany \
    -nostdlib -nostartfiles -ffreestanding -fno-common \
    -O0 -Wall -Wextra \
    -Iinclude \
    -c tests/user_test.c -o build/user_test.o

# Link user test (no special linker script, just load at 0)
echo "Linking user test program..."
$LD -m elf64lriscv -Ttext=0x10000 \
    build/user_test.o -o build/user_test.elf

# Convert to binary
echo "Creating user test binary..."
$OBJCOPY -O binary build/user_test.elf build/user_test.bin

echo "User test program built: build/user_test.bin"
echo "Size: $(wc -c < build/user_test.bin) bytes"
