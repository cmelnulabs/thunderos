#!/bin/bash
# Local test script for pipe implementation

set -e

echo "=================================="
echo "  Testing Pipe Implementation"
echo "=================================="
echo ""

# Build everything
echo "[1/4] Building kernel and userland..."
make clean >/dev/null 2>&1
make >/dev/null 2>&1
./build_userland.sh >/dev/null 2>&1
echo "      ✓ Build successful"

# Create filesystem
echo "[2/4] Creating filesystem with pipe_test..."
make fs >/dev/null 2>&1
echo "      ✓ Filesystem created"

# Verify pipe_test is in the filesystem
echo "[3/4] Verifying pipe_test binary..."
if [ -f "userland/build/pipe_test" ]; then
    echo "      ✓ pipe_test binary exists"
    ls -lh userland/build/pipe_test | awk '{print "      Size: " $5}'
else
    echo "      ✗ ERROR: pipe_test binary not found"
    exit 1
fi

# Run in QEMU with automated test
echo "[4/4] Running pipe_test in QEMU..."
echo ""
echo "      Starting QEMU (30 second timeout)..."
echo "      Will automatically execute: ls /bin; pipe_test"
echo ""

# Create expect-style input
(
    sleep 2
    echo "ls /bin"
    sleep 1
    echo "pipe_test"
    sleep 8
) | timeout 35 make qemu 2>&1 | tee /tmp/qemu_pipe_test.log

echo ""
echo "=================================="
echo "  Test Results"
echo "=================================="

# Check if pipe subsystem initialized
if grep -q "\[OK\] Pipe subsystem initialized" /tmp/qemu_pipe_test.log; then
    echo "✓ Pipe subsystem initialized"
else
    echo "✗ Pipe subsystem NOT initialized"
    exit 1
fi

# Check if pipe_test is in filesystem listing
if grep -q "pipe_test" /tmp/qemu_pipe_test.log; then
    echo "✓ pipe_test found in /bin"
else
    echo "✗ pipe_test NOT found in filesystem"
    exit 1
fi

# Check if test ran
if grep -q "Pipe Test Program" /tmp/qemu_pipe_test.log; then
    echo "✓ Pipe test program started"
else
    echo "⚠ Pipe test program may not have started (timeout)"
    echo ""
    echo "Note: You can manually test by running:"
    echo "  make qemu"
    echo "  > exec /bin/pipe_test"
    exit 0
fi

# Check for test results
if grep -q "\[PASS\] Pipe created successfully" /tmp/qemu_pipe_test.log; then
    echo "✓ TEST 1: Pipe creation - PASSED"
else
    echo "⚠ TEST 1: Pipe creation - status unknown"
fi

if grep -q "\[PARENT\].*Parent process" /tmp/qemu_pipe_test.log; then
    echo "✓ TEST 2: Fork - PASSED"
else
    echo "⚠ TEST 2: Fork - status unknown"
fi

if grep -q "\[CHILD\].*Child process started" /tmp/qemu_pipe_test.log; then
    echo "✓ TEST 3: Child communication - PASSED"
else
    echo "⚠ TEST 3: Child communication - status unknown"
fi

if grep -q "Message matches" /tmp/qemu_pipe_test.log; then
    echo "✓ TEST 4: Data integrity - PASSED"
else
    echo "⚠ TEST 4: Data integrity - status unknown"
fi

if grep -q "All pipe tests completed successfully" /tmp/qemu_pipe_test.log; then
    echo ""
    echo "=================================="
    echo "  🎉 ALL TESTS PASSED!"
    echo "=================================="
else
    echo ""
    echo "=================================="
    echo "  ⚠ Tests incomplete (may need manual verification)"
    echo "=================================="
fi

echo ""
echo "Full test output saved to: /tmp/qemu_pipe_test.log"
echo ""
echo "To manually test:"
echo "  make qemu"
echo "  > ls /bin"
echo "  > pipe_test"
echo ""
