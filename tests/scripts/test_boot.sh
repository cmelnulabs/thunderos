#!/bin/bash
#
# ThunderOS Boot Test
# Quick test to verify kernel boots and initializes correctly
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/boot_test_output.txt"
QEMU_TIMEOUT=5

# QEMU 10.1.2+ required for SSTC extension support
QEMU_BIN="${QEMU_BIN:-/tmp/qemu-10.1.2/build/qemu-system-riscv64}"

# Create output directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo "========================================"
    echo "  $1"
    echo "========================================"
    echo ""
}

print_pass() {
    echo -e "  ${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "  ${RED}[FAIL]${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}[INFO]${NC} $1"
}

print_test() {
    echo -e "\n${YELLOW}[TEST]${NC} $1"
}

# Build kernel
print_header "ThunderOS Boot Test"
print_info "Building kernel..."

cd "${ROOT_DIR}"
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_pass "Kernel build successful"
else
    print_fail "Kernel build failed"
    exit 1
fi

# Verify ELF exists
if [ ! -f "${BUILD_DIR}/thunderos.elf" ]; then
    print_fail "Kernel ELF not found"
    exit 1
fi
print_pass "Kernel ELF verified"

# Run QEMU
print_test "Booting kernel in QEMU (${QEMU_TIMEOUT}s timeout)"

timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    </dev/null 2>&1 | tee "${OUTPUT_FILE}" || true

# Analyze output
print_test "Analyzing boot sequence"

FAILED=0

# Test 1: Kernel boots
if grep -q "ThunderOS\|Kernel loaded" "${OUTPUT_FILE}"; then
    print_pass "Kernel started"
else
    print_fail "Kernel did not start"
    FAILED=$((FAILED + 1))
fi

# Test 2: UART initialized
if grep -q "\[OK\] UART initialized" "${OUTPUT_FILE}"; then
    print_pass "UART initialized"
else
    print_fail "UART initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 3: Interrupts enabled
if grep -q "\[OK\] Interrupt\|interrupts enabled" "${OUTPUT_FILE}"; then
    print_pass "Interrupts enabled"
else
    print_fail "Interrupt subsystem failed"
    FAILED=$((FAILED + 1))
fi

# Test 4: Memory management
if grep -q "\[OK\] Memory management\|PMM: Initialized" "${OUTPUT_FILE}"; then
    print_pass "Memory management initialized"
else
    print_fail "Memory management failed"
    FAILED=$((FAILED + 1))
fi

# Test 5: Virtual memory
if grep -q "\[OK\] Virtual memory\|Paging enabled" "${OUTPUT_FILE}"; then
    print_pass "Virtual memory enabled"
else
    print_fail "Virtual memory failed"
    FAILED=$((FAILED + 1))
fi

# Test 6: Process management
if grep -q "\[OK\] Process management\|Scheduler initialized" "${OUTPUT_FILE}"; then
    print_pass "Process and scheduler initialized"
else
    print_fail "Process/scheduler initialization failed"
    FAILED=$((FAILED + 1))
fi

# Summary
print_header "Boot Test Summary"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All boot tests passed (6/6)${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 0
else
    echo -e "${RED}✗ $FAILED test(s) failed${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 1
fi
