#!/bin/bash
# Test user mode execution with automatic exit

echo "Testing ThunderOS user mode execution..."
echo "=========================================="

# Create a simple EOF signal after 2 seconds
(sleep 2; echo) | timeout 5 qemu-system-riscv64 \
    -machine virt -m 128M -nographic -serial mon:stdio \
    -bios default -kernel build/thunderos.elf 2>&1

exit_code=$?

echo
echo "=========================================="
echo "Test completed (exit code: $exit_code)"
