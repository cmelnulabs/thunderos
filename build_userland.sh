#!/bin/bash
# Build userland programs for ThunderOS

set -euo pipefail

readonly CROSS_COMPILE="riscv64-unknown-elf-"
readonly CC="${CROSS_COMPILE}gcc"
readonly LD="${CROSS_COMPILE}ld"
readonly OBJCOPY="${CROSS_COMPILE}objcopy"

readonly USERLAND_DIR="$(cd "$(dirname "$0")" && pwd)/userland"
readonly BUILD_DIR="${USERLAND_DIR}/build"

readonly CFLAGS="-march=rv64gc -mabi=lp64d -nostdlib -nostartfiles -ffreestanding -fno-common -O0 -g -Wall"
readonly LDFLAGS="-nostdlib -static"
readonly LDSCRIPT="${USERLAND_DIR}/user.ld"

# User program entry point address
#readonly USER_ENTRY_POINT="0xf000"

# Create build directory
mkdir -p "${BUILD_DIR}"

echo "Building userland programs..."

# Build ls
echo "Building ls..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/ls.c" -o "${BUILD_DIR}/ls.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/ls.o" -o "${BUILD_DIR}/ls"
"${OBJCOPY}" -O binary "${BUILD_DIR}/ls" "${BUILD_DIR}/ls.bin"

# Build cat
echo "Building cat..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/cat.c" -o "${BUILD_DIR}/cat.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/cat.o" -o "${BUILD_DIR}/cat"
"${OBJCOPY}" -O binary "${BUILD_DIR}/cat" "${BUILD_DIR}/cat.bin"

# Build hello
echo "Building hello..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/hello.c" -o "${BUILD_DIR}/hello.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/hello.o" -o "${BUILD_DIR}/hello"
"${OBJCOPY}" -O binary "${BUILD_DIR}/hello" "${BUILD_DIR}/hello.bin"

# Build signal_test
echo "Building signal_test..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/signal_test.c" -o "${BUILD_DIR}/signal_test.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/signal_test.o" -o "${BUILD_DIR}/signal_test"
"${OBJCOPY}" -O binary "${BUILD_DIR}/signal_test" "${BUILD_DIR}/signal_test.bin"

# Build pipe_test
echo "Building pipe_test..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/pipe_test.c" -o "${BUILD_DIR}/pipe_test.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/pipe_test.o" -o "${BUILD_DIR}/pipe_test"
"${OBJCOPY}" -O binary "${BUILD_DIR}/pipe_test" "${BUILD_DIR}/pipe_test.bin"

# Build pipe_simple_test
echo "Building pipe_simple_test..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/pipe_simple_test.c" -o "${BUILD_DIR}/pipe_simple_test.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/pipe_simple_test.o" -o "${BUILD_DIR}/pipe_simple_test"
"${OBJCOPY}" -O binary "${BUILD_DIR}/pipe_simple_test" "${BUILD_DIR}/pipe_simple_test.bin"

# Build ush (user shell) - flat version to avoid stack issues with -O0
echo "Building ush (flat)..."
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/ush_flat.c" -o "${BUILD_DIR}/ush_flat.o"
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/syscall.S" -o "${BUILD_DIR}/syscall.o"
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/ush_flat.o" "${BUILD_DIR}/syscall.o" -o "${BUILD_DIR}/ush"
"${OBJCOPY}" -O binary "${BUILD_DIR}/ush" "${BUILD_DIR}/ush.bin"

echo "Userland programs built successfully!"
