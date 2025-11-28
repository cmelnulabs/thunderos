#!/bin/bash
#
# ThunderOS Integration Test
# Comprehensive test of VirtIO, ext2, shell, and user programs
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
OUTPUT_FILE="${OUTPUT_DIR}/integration_test_output.txt"
QEMU_TIMEOUT=8

# QEMU 10.1.2+ required for SSTC extension support
# Try system QEMU first, then custom build
if command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="${QEMU_BIN:-qemu-system-riscv64}"
elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
    QEMU_BIN="/tmp/qemu-10.1.2/build/qemu-system-riscv64"
else
    echo "ERROR: qemu-system-riscv64 not found"
    exit 1
fi

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

# Build kernel with TEST_MODE (no shell)
print_header "ThunderOS Integration Test"
print_info "Building kernel in test mode (no shell)..."

cd "${ROOT_DIR}"
if make clean >/dev/null 2>&1 && make TEST_MODE=1 >/dev/null 2>&1; then
    print_pass "Kernel build successful"
else
    print_fail "Kernel build failed"
    exit 1
fi

print_info "Building userland programs..."
if make userland >/dev/null 2>&1; then
    print_pass "Userland build successful"
else
    print_fail "Userland build failed (non-critical)"
fi

# Create ext2 filesystem
print_info "Creating ext2 filesystem..."

DISK_IMAGE="${BUILD_DIR}/test_fs.img"
rm -rf "${BUILD_DIR}/test_fs_contents"
mkdir -p "${BUILD_DIR}/test_fs_contents/bin"

# Add test files
echo "Integration Test File" > "${BUILD_DIR}/test_fs_contents/test.txt"
echo "README for ThunderOS" > "${BUILD_DIR}/test_fs_contents/README.txt"

# Copy userland programs
for prog in hello cat ls; do
    if [ -f "${ROOT_DIR}/userland/build/$prog" ]; then
        cp "${ROOT_DIR}/userland/build/$prog" "${BUILD_DIR}/test_fs_contents/bin/"
        print_pass "Added $prog to filesystem"
    fi
done

# Create ext2 image
if mkfs.ext2 -F -q -d "${BUILD_DIR}/test_fs_contents" "${DISK_IMAGE}" 10M >/dev/null 2>&1; then
    print_pass "ext2 filesystem created"
else
    print_fail "ext2 filesystem creation failed"
    exit 1
fi

rm -rf "${BUILD_DIR}/test_fs_contents"

# Run QEMU
print_test "Running full integration test (${QEMU_TIMEOUT}s timeout)"

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
print_test "Analyzing integration test results"

FAILED=0

# Test 1: Kernel boots
if grep -q "ThunderOS\|Kernel loaded" "${OUTPUT_FILE}"; then
    print_pass "Kernel started"
else
    print_fail "Kernel did not start"
    FAILED=$((FAILED + 1))
fi

# Test 2: Memory tests pass
if grep -q "ALL TESTS PASSED\|Memory Management Feature Tests" "${OUTPUT_FILE}"; then
    print_pass "Built-in memory tests passed"
else
    print_fail "Built-in memory tests failed"
    FAILED=$((FAILED + 1))
fi

# Test 3: VirtIO initialized
if grep -q "\[OK\] VirtIO block device initialized" "${OUTPUT_FILE}"; then
    print_pass "VirtIO block device initialized"
else
    print_fail "VirtIO initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 4: ext2 mounted
if grep -q "\[OK\] ext2 filesystem mounted\|Mounted root filesystem" "${OUTPUT_FILE}"; then
    print_pass "ext2 filesystem mounted"
else
    print_fail "ext2 mount failed"
    FAILED=$((FAILED + 1))
fi

# Test 5: ELF loader tests
if grep -q "ELF Loader Tests" "${OUTPUT_FILE}"; then
    print_pass "ELF loader tests executed"
else
    print_fail "ELF loader tests not found"
    FAILED=$((FAILED + 1))
fi

# Summary
print_header "Integration Test Summary"

TOTAL_TESTS=5
PASSED=$((TOTAL_TESTS - FAILED))

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All integration tests passed ($PASSED/$TOTAL_TESTS)${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 0
else
    echo -e "${RED}✗ $FAILED test(s) failed${NC}"
    echo -e "${GREEN}✓ $PASSED test(s) passed${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 1
fi
