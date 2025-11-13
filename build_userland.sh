#!/bin/bash
# Build userland programs for ThunderOS

set -e

CROSS_COMPILE=riscv64-unknown-elf-
CC="${CROSS_COMPILE}gcc"
LD="${CROSS_COMPILE}ld"
OBJCOPY="${CROSS_COMPILE}objcopy"

CFLAGS="-march=rv64gc -mabi=lp64d -nostdlib -nostartfiles -ffreestanding -fno-common -O2 -Wall"
LDFLAGS="-nostdlib -static"

USERLAND_DIR="$(cd "$(dirname "$0")" && pwd)/userland"
BUILD_DIR="${USERLAND_DIR}/build"

# Create build directory
mkdir -p "${BUILD_DIR}"

echo "Building userland programs..."

# Build ls
echo "Building ls..."
${CC} ${CFLAGS} -c "${USERLAND_DIR}/ls.c" -o "${BUILD_DIR}/ls.o"
${LD} ${LDFLAGS} -Ttext=0xf000 "${BUILD_DIR}/ls.o" -o "${BUILD_DIR}/ls"
${OBJCOPY} -O binary "${BUILD_DIR}/ls" "${BUILD_DIR}/ls.bin"

# Build cat
echo "Building cat..."
${CC} ${CFLAGS} -c "${USERLAND_DIR}/cat.c" -o "${BUILD_DIR}/cat.o"
${LD} ${LDFLAGS} -Ttext=0xf000 "${BUILD_DIR}/cat.o" -o "${BUILD_DIR}/cat"
${OBJCOPY} -O binary "${BUILD_DIR}/cat" "${BUILD_DIR}/cat.bin"

# Build hello
echo "Building hello..."
${CC} ${CFLAGS} -c "${USERLAND_DIR}/hello.c" -o "${BUILD_DIR}/hello.o"
${LD} ${LDFLAGS} -Ttext=0xf000 "${BUILD_DIR}/hello.o" -o "${BUILD_DIR}/hello"
${OBJCOPY} -O binary "${BUILD_DIR}/hello" "${BUILD_DIR}/hello.bin"

echo "Userland programs built successfully!"
ls -lh "${BUILD_DIR}/"
