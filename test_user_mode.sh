#!/bin/bash
# Test user mode execution in QEMU with timeout

echo "Starting ThunderOS user mode test..."
echo "Running for 10 seconds to observe scheduler and user processes..."
echo "=========================================="
echo

timeout 10 qemu-system-riscv64 -machine virt -m 128M -nographic -serial mon:stdio \
    -bios default -kernel build/thunderos.elf 2>&1

exit_code=$?

echo
echo "=========================================="
if [ $exit_code -eq 124 ]; then
    echo "Test completed (timeout after 10 seconds)"
else
    echo "Test completed with exit code: $exit_code"
fi
