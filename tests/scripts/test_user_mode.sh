#!/bin/bash
# Test user mode execution
# Tests that userland programs can execute in user mode

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$WORKSPACE_DIR"

echo "==================================="
echo "  User Mode Execution Test"
echo "==================================="

# Build kernel
echo "Building kernel..."
make -s clean && make -s || exit 1

# Build userland programs
echo "Building userland programs..."
./build_userland.sh || exit 1

# Create test filesystem
echo "Creating test filesystem..."
cd build
mkdir -p testfs
echo "Hello from ext2!" > testfs/hello.txt
echo "Testing user mode" > testfs/test.txt
mkfs.ext2 -F -q -d testfs ext2-disk.img 10M
cd ..

# Start QEMU and feed commands
echo "Running QEMU tests..."
{
    sleep 3  # Wait for boot
    echo "ls /"
    sleep 2
    echo "cat /hello.txt"
    sleep 2
    echo "exit"
} | timeout 20 qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/ext2-disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 2>&1 | tee test_output.txt

echo ""
echo "==================================="
echo "  Test Results"
echo "==================================="

TESTS_PASSED=0
TESTS_FAILED=0

# Check ls command
if grep -q "lost+found" test_output.txt && grep -q "hello.txt" test_output.txt; then
    echo "✓ ls command executed successfully"
    ((TESTS_PASSED++))
else
    echo "✗ ls command failed"
    ((TESTS_FAILED++))
fi

# Check cat command
if grep -q "Hello from ext2" test_output.txt; then
    echo "✓ cat command executed successfully"
    ((TESTS_PASSED++))
else
    echo "✗ cat command failed"
    ((TESTS_FAILED++))
fi

# Check for kernel panics or crashes
if grep -q "KERNEL PANIC" test_output.txt || grep -q "page fault" test_output.txt; then
    echo "✗ Kernel panic or page fault detected"
    ((TESTS_FAILED++))
else
    echo "✓ No kernel panics or page faults"
    ((TESTS_PASSED++))
fi

echo ""
echo "==================================="
echo "Passed: $TESTS_PASSED  Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "*** ALL TESTS PASSED ***"
    exit 0
else
    echo "*** SOME TESTS FAILED ***"
    exit 1
fi
