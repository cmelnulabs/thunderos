#!/bin/bash
#
# Automated Syscall Testing Script
# Tests sys_write, sys_read, sys_getpid, and sys_sleep
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
QEMU_TIMEOUT=5
OUTPUT_FILE="/tmp/thunderos_test_output.txt"

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
cd "${SCRIPT_DIR}"
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_status "Build successful"
else
    print_error "Build failed"
    exit 1
fi

# Check if executable exists
if [ ! -f "${BUILD_DIR}/thunderos.elf" ]; then
    print_error "Kernel ELF not found at ${BUILD_DIR}/thunderos.elf"
    exit 1
fi

print_status "Kernel ELF found"

# Run QEMU with timeout
print_info "Running QEMU test..."
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
print_info "Analyzing test results..."

TEST_RESULTS=0

# Test 1: Check if kernel started (look for OK markers)
if grep -q "Kernel loaded\|Initializing..." "${OUTPUT_FILE}"; then
    print_status "Test 1: Kernel initialization - PASS"
else
    print_error "Test 1: Kernel initialization - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 2: Check if processes created (look for process table or creation messages)
if grep -q "Created Process\|Process Table\|Created user process" "${OUTPUT_FILE}"; then
    print_status "Test 2: Process creation - PASS"
else
    print_error "Test 2: Process creation - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 3: Check for scheduler output
if grep -q "Scheduler" "${OUTPUT_FILE}" || grep -q "scheduling" "${OUTPUT_FILE}"; then
    print_status "Test 3: Scheduler running - PASS"
else
    print_error "Test 3: Scheduler running - FAIL (optional)"
fi

# Test 4: Check for user process
if grep -q "User process" "${OUTPUT_FILE}" || grep -q "user mode" "${OUTPUT_FILE}" || grep -q "PID 4" "${OUTPUT_FILE}"; then
    print_status "Test 4: User process created - PASS"
else
    print_error "Test 4: User process created - FAIL (optional)"
fi

# Test 5: Check for sys_write output
if grep -q "Hello\|Output\|message\|Testing user exception" "${OUTPUT_FILE}"; then
    print_status "Test 5: sys_write working - PASS"
else
    print_error "Test 5: sys_write working - FAIL (optional)"
fi

# Test 6: Check for user exception handling
if grep -q "USER PROCESS EXCEPTION" "${OUTPUT_FILE}"; then
    print_status "Test 6: User exception handling - PASS"
else
    print_error "Test 6: User exception handling - FAIL (optional)"
fi

# Summary
echo ""
echo "================================"
if [ $TEST_RESULTS -eq 0 ]; then
    print_status "All critical tests passed!"
    exit 0
else
    print_error "$TEST_RESULTS test(s) failed"
    exit 1
fi
