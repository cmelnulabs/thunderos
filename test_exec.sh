#!/bin/bash

echo "Testing execve syscall..."

# Run QEMU and send exec_test command
timeout 12 qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios none \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/fs.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 2>&1 | tee exec_test_output.log &

QEMU_PID=$!

# Wait for shell to be ready
sleep 3

# Send the exec_test command
echo "exec_test" > /proc/$QEMU_PID/fd/0 2>/dev/null || true

# Wait for test to complete
sleep 5

# Kill QEMU
kill -9 $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo ""
echo "========================================="
echo "Exec Test Output:"
echo "========================================="
grep -A 20 "exec_test" exec_test_output.log | head -30
