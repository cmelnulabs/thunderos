#!/bin/bash
#
# Quick User Mode Testing Script
# Fast automated test for user-mode functionality
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
QEMU_TIMEOUT=3
OUTPUT_FILE="${SCRIPT_DIR}/thunderos_quick_test_output.txt"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[*]${NC} $1"
}

# Build the kernel
print_info "Building kernel..."
cd "${SCRIPT_DIR}/.."
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_status "Build successful"
else
    print_error "Build failed"
    exit 1
fi
cd "${SCRIPT_DIR}"

# Check if executable exists
if [ ! -f "${BUILD_DIR}/thunderos.elf" ]; then
    print_error "Kernel ELF not found at ${BUILD_DIR}/thunderos.elf"
    exit 1
fi

print_status "Kernel ELF found"

# Run QEMU with timeout
print_info "Running quick user mode test..."
{
    sleep "${QEMU_TIMEOUT}"
    echo ""
} | timeout $((QEMU_TIMEOUT + 2)) qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel "${BUILD_DIR}/thunderos.elf" 2>&1 | tee "${OUTPUT_FILE}"

print_status "QEMU execution completed (output saved to ${OUTPUT_FILE})"

# Analyze output
print_info "Analyzing quick test results..."

TEST_RESULTS=0

# Test 1: Check if kernel started
if grep -q "Kernel loaded\|Initializing..." "${OUTPUT_FILE}"; then
    print_status "Test 1: Kernel initialization - PASS"
else
    print_error "Test 1: Kernel initialization - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 2: Check for user processes
if grep -q "User process\|user mode\|PID" "${OUTPUT_FILE}"; then
    print_status "Test 2: User processes running - PASS"
else
    print_error "Test 2: User processes running - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 3: Check for system calls
if grep -q "syscall\|sys_write\|Hello" "${OUTPUT_FILE}"; then
    print_status "Test 3: System calls working - PASS"
else
    print_error "Test 3: System calls working - FAIL (optional)"
fi

# Test 4: Check for scheduler
if grep -q "Scheduler\|scheduling" "${OUTPUT_FILE}"; then
    print_status "Test 4: Scheduler operational - PASS"
else
    print_error "Test 4: Scheduler operational - FAIL (optional)"
fi

# Summary
echo ""
echo "================================"
if [ $TEST_RESULTS -eq 0 ]; then
    print_status "All critical quick tests passed!"
    exit 0
else
    print_error "$TEST_RESULTS test(s) failed"
    exit 1
fi
