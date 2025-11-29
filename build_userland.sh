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

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly CYAN='\033[0;36m'
readonly BOLD='\033[1m'
readonly NC='\033[0m' # No Color

# Counters
TOTAL_PROGRAMS=0
BUILT_PROGRAMS=0

# Print functions
print_header() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}${BOLD}           ThunderOS Userland Build System                    ${NC}${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_section() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_building() {
    printf "  ${YELLOW}▶${NC} Building %-20s " "$1"
}

print_success() {
    echo -e "${GREEN}✓${NC}"
    BUILT_PROGRAMS=$((BUILT_PROGRAMS + 1))
}

print_footer() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}${BOLD}  ✓ Build Complete!${NC}"
    echo -e "    Programs built: ${BOLD}${BUILT_PROGRAMS}/${TOTAL_PROGRAMS}${NC}"
    echo -e "    Output directory: ${CYAN}${BUILD_DIR}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# Build function for simple programs (single source file)
build_program() {
    local name="$1"
    local source="${2:-$1}"  # Use name as source if not specified
    
    TOTAL_PROGRAMS=$((TOTAL_PROGRAMS + 1))
    print_building "$name"
    
    "${CC}" ${CFLAGS} -c "${USERLAND_DIR}/${source}.c" -o "${BUILD_DIR}/${source}.o" 2>/dev/null
    "${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/${source}.o" -o "${BUILD_DIR}/${name}" 2>/dev/null
    "${OBJCOPY}" -O binary "${BUILD_DIR}/${name}" "${BUILD_DIR}/${name}.bin" 2>/dev/null
    
    print_success
}

# Create build directory
mkdir -p "${BUILD_DIR}"

print_header

# Core utilities
print_section "Core Utilities"
build_program "ls"
build_program "cat"
build_program "pwd"
build_program "mkdir"
build_program "rmdir"
build_program "touch"
build_program "rm"
build_program "clear"
build_program "sleep"
build_program "ps"
build_program "uname"
build_program "uptime"
build_program "whoami"
build_program "tty"
build_program "kill"

# User applications
print_section "User Applications"
build_program "hello"
build_program "clock"

# Build ush (user shell) - special case with multiple source files
TOTAL_PROGRAMS=$((TOTAL_PROGRAMS + 1))
print_building "ush (shell)"
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/ush.c" -o "${BUILD_DIR}/ush.o" 2>/dev/null
"${CC}" ${CFLAGS} -c "${USERLAND_DIR}/syscall.S" -o "${BUILD_DIR}/syscall.o" 2>/dev/null
"${LD}" ${LDFLAGS} -T"${LDSCRIPT}" "${BUILD_DIR}/ush.o" "${BUILD_DIR}/syscall.o" -o "${BUILD_DIR}/ush" 2>/dev/null
"${OBJCOPY}" -O binary "${BUILD_DIR}/ush" "${BUILD_DIR}/ush.bin" 2>/dev/null
print_success

# Test programs
print_section "Test Programs"
build_program "signal_test"
build_program "pipe_test"
build_program "pipe_simple_test"

print_footer
