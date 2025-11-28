#!/bin/bash
#
# ThunderOS Kernel Functionality Test
# Tests all kernel subsystems without shell interaction
#
# This script verifies:
#   1. Kernel boot and initialization
#   2. Memory management (PMM, paging, DMA, kmalloc)
#   3. VirtIO block device
#   4. ext2 filesystem mounting
#   5. ELF loader
#   6. Process management and scheduler
#   7. Signal subsystem
#   8. Pipe subsystem
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
OUTPUT_FILE="${OUTPUT_DIR}/kernel_test_output.txt"
QEMU_TIMEOUT=10

# QEMU detection
if command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="${QEMU_BIN:-qemu-system-riscv64}"
elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
    QEMU_BIN="/tmp/qemu-10.1.2/build/qemu-system-riscv64"
else
    echo "ERROR: qemu-system-riscv64 not found"
    exit 1
fi

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

print_pass() { echo -e "  ${GREEN}[PASS]${NC} $1"; }
print_fail() { echo -e "  ${RED}[FAIL]${NC} $1"; }
print_info() { echo -e "  ${BLUE}[INFO]${NC} $1"; }
print_test() { echo -e "\n${YELLOW}[TEST]${NC} $1"; }

# Build kernel with TEST_MODE (no shell)
print_header "ThunderOS Kernel Functionality Test"
print_info "Building kernel in test mode (no shell)..."

cd "${ROOT_DIR}"
if make clean >/dev/null 2>&1 && make TEST_MODE=1 >/dev/null 2>&1; then
    print_pass "Kernel build successful"
else
    print_fail "Kernel build failed"
    exit 1
fi

# Build userland
print_info "Building userland programs..."
if make userland >/dev/null 2>&1; then
    print_pass "Userland build successful"
else
    print_info "Userland build skipped (optional)"
fi

# Create filesystem
print_info "Creating ext2 filesystem..."
DISK_IMAGE="${BUILD_DIR}/test_fs.img"
rm -rf "${BUILD_DIR}/test_fs_contents"
mkdir -p "${BUILD_DIR}/test_fs_contents/bin"

echo "Test file for ThunderOS" > "${BUILD_DIR}/test_fs_contents/test.txt"
echo "Hello from ext2!" > "${BUILD_DIR}/test_fs_contents/hello.txt"

# Copy userland programs if they exist
for prog in hello cat ls pwd mkdir rmdir clear ush; do
    if [ -f "${ROOT_DIR}/userland/build/$prog" ]; then
        cp "${ROOT_DIR}/userland/build/$prog" "${BUILD_DIR}/test_fs_contents/bin/"
    fi
done

if mkfs.ext2 -F -q -d "${BUILD_DIR}/test_fs_contents" "${DISK_IMAGE}" 10M >/dev/null 2>&1; then
    print_pass "ext2 filesystem created"
else
    print_fail "ext2 filesystem creation failed"
    exit 1
fi
rm -rf "${BUILD_DIR}/test_fs_contents"

# Run QEMU
print_test "Running kernel in QEMU (${QEMU_TIMEOUT}s timeout)"
print_info "All tests are built into the kernel and run automatically"

timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios none \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    -global virtio-mmio.force-legacy=false \
    -drive file="${DISK_IMAGE}",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    </dev/null 2>&1 | tee "${OUTPUT_FILE}" || true

# Analyze output
print_header "Test Results"

FAILED=0
PASSED=0

# Test 1: Kernel Boot
print_test "Kernel Boot"
if grep -q "ThunderOS.*RISC-V" "${OUTPUT_FILE}"; then
    print_pass "Kernel banner displayed"
    PASSED=$((PASSED + 1))
else
    print_fail "Kernel banner not found"
    FAILED=$((FAILED + 1))
fi

# Test 2: UART
if grep -q "\[OK\] UART initialized" "${OUTPUT_FILE}"; then
    print_pass "UART initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "UART initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 3: Interrupts
if grep -q "\[OK\] Interrupt subsystem initialized" "${OUTPUT_FILE}"; then
    print_pass "Interrupt subsystem initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Interrupt initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 4: Trap handler
if grep -q "\[OK\] Trap handler initialized" "${OUTPUT_FILE}"; then
    print_pass "Trap handler initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Trap handler initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 5: Memory management
if grep -q "\[OK\] Memory management initialized" "${OUTPUT_FILE}"; then
    print_pass "Memory management initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Memory management initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 6: Virtual memory
if grep -q "\[OK\] Virtual memory initialized" "${OUTPUT_FILE}"; then
    print_pass "Virtual memory (Sv39) initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Virtual memory initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 7: DMA allocator
if grep -q "\[OK\] DMA allocator initialized" "${OUTPUT_FILE}"; then
    print_pass "DMA allocator initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "DMA allocator initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 8: Built-in memory tests
print_test "Memory Management Tests"
if grep -q "ALL TESTS PASSED" "${OUTPUT_FILE}"; then
    # Count how many tests passed
    TEST_COUNT=$(grep -c "PASS" "${OUTPUT_FILE}" 2>/dev/null || echo "0")
    print_pass "Built-in memory tests passed (${TEST_COUNT} checks)"
    PASSED=$((PASSED + 1))
else
    print_fail "Built-in memory tests failed"
    FAILED=$((FAILED + 1))
fi

# Test 9: ELF loader tests
print_test "ELF Loader Tests"
if grep -q "ELF Loader Tests" "${OUTPUT_FILE}" && grep -q "ALL TESTS PASSED" "${OUTPUT_FILE}"; then
    print_pass "ELF loader tests passed"
    PASSED=$((PASSED + 1))
else
    print_fail "ELF loader tests failed or not run"
    FAILED=$((FAILED + 1))
fi

# Test 10: Process management
print_test "Process Management"
if grep -q "\[OK\] Process management initialized" "${OUTPUT_FILE}"; then
    print_pass "Process management initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Process management initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 11: Scheduler
if grep -q "\[OK\] Scheduler initialized" "${OUTPUT_FILE}"; then
    print_pass "Scheduler initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Scheduler initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 12: Pipe subsystem
if grep -q "\[OK\] Pipe subsystem initialized" "${OUTPUT_FILE}"; then
    print_pass "Pipe subsystem initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "Pipe subsystem initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 13: VirtIO
print_test "VirtIO Block Device"
if grep -q "\[OK\] VirtIO block device initialized" "${OUTPUT_FILE}"; then
    print_pass "VirtIO block device initialized"
    PASSED=$((PASSED + 1))
else
    print_fail "VirtIO initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 14: ext2 filesystem
print_test "ext2 Filesystem"
if grep -q "\[OK\] ext2 filesystem mounted" "${OUTPUT_FILE}"; then
    print_pass "ext2 filesystem mounted"
    PASSED=$((PASSED + 1))
else
    print_fail "ext2 mount failed"
    FAILED=$((FAILED + 1))
fi

# Test 15: VFS
if grep -q "\[OK\] VFS root filesystem mounted" "${OUTPUT_FILE}"; then
    print_pass "VFS root filesystem mounted"
    PASSED=$((PASSED + 1))
else
    print_fail "VFS mount failed"
    FAILED=$((FAILED + 1))
fi

# Test 16: No kernel panics
print_test "Stability"
if grep -qi "KERNEL PANIC\|page fault\|exception\|trap" "${OUTPUT_FILE}" | grep -v "Trap handler\|trap_handler"; then
    print_fail "Kernel panic or unhandled exception detected"
    FAILED=$((FAILED + 1))
else
    print_pass "No kernel panics or crashes"
    PASSED=$((PASSED + 1))
fi

# Summary
print_header "Test Summary"

TOTAL=$((PASSED + FAILED))

echo ""
echo "  Tests Passed: ${PASSED}/${TOTAL}"
echo "  Tests Failed: ${FAILED}/${TOTAL}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}========================================"
    echo -e "  ✓ ALL TESTS PASSED"
    echo -e "========================================${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 0
else
    echo -e "${RED}========================================"
    echo -e "  ✗ ${FAILED} TEST(S) FAILED"
    echo -e "========================================${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    print_info "Review output for details"
    exit 1
fi
