#!/bin/bash

# Test the user-mode shell interactively

echo "Testing ThunderOS User-Mode Shell..."
echo ""

# Start QEMU and send commands
(
    sleep 3
    echo "help"
    sleep 1
    echo "echo Hello from user mode"
    sleep 1
    echo "ls"
    sleep 1
    echo "cat /test.txt"
    sleep 1
    echo "hello"
    sleep 2
    echo "exit"
    sleep 1
) | timeout 20 qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios none \
    -kernel /workspace/build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=/workspace/build/fs.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 2>&1 | tee /tmp/shell_test.log

echo ""
echo "===== Test Results ====="
grep -A 20 "ThunderOS>" /tmp/shell_test.log | head -50
