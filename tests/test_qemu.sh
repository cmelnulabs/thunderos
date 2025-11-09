#!/bin/bash
#
# QEMU Testing Script
# Basic build and QEMU execution test
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
QEMU_TIMEOUT=5
OUTPUT_FILE="${SCRIPT_DIR}/thunderos_qemu_output.txt"

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

# Check QEMU availability
print_info "Checking QEMU installation..."
if ! command -v qemu-system-riscv64 &> /dev/null; then
    print_error "qemu-system-riscv64 not found"
    exit 1
fi

if ! qemu-system-riscv64 --version &> /dev/null; then
    print_error "qemu-system-riscv64 is not executable or does not support RISC-V 64-bit"
    exit 1
fi

print_status "QEMU RISC-V 64-bit support available"

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
print_info "Analyzing QEMU test results..."

TEST_RESULTS=0

# Test 1: Check if kernel started
if grep -q "Kernel loaded\|Initializing..." "${OUTPUT_FILE}"; then
    print_status "Test 1: Kernel initialization - PASS"
else
    print_error "Test 1: Kernel initialization - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 2: Check for basic system functionality
if grep -q "ThunderOS\|kernel\|boot" "${OUTPUT_FILE}"; then
    print_status "Test 2: Basic system functionality - PASS"
else
    print_error "Test 2: Basic system functionality - FAIL (optional)"
fi

# Test 3: Check for processes
if grep -q "Process\|scheduler\|PID" "${OUTPUT_FILE}"; then
    print_status "Test 3: Process management active - PASS"
else
    print_error "Test 3: Process management active - FAIL (optional)"
fi

# Test 4: Check for memory management
if grep -q "memory\|PMM\|kmalloc\|paging" "${OUTPUT_FILE}"; then
    print_status "Test 4: Memory management operational - PASS"
else
    print_error "Test 4: Memory management operational - FAIL (optional)"
fi

# Summary
echo ""
echo "================================"
if [ $TEST_RESULTS -eq 0 ]; then
    print_status "All critical QEMU tests passed!"
    exit 0
else
    print_error "$TEST_RESULTS test(s) failed"
    exit 1
fi
