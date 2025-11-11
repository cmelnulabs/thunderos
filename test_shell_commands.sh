#!/bin/bash
# Test shell commands with ext2 filesystem

cd /workspaces/thunderos

echo "Starting QEMU with ext2 filesystem..."
echo "Commands to test: help, ls, ls /, cat /test.txt, ls /bin"
echo "Press Ctrl-A then X to exit QEMU"
echo ""

qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/ext2-disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0
