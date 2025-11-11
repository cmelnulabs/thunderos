#!/bin/bash
# Automated test for program execution from disk

set -e

cd /workspaces/thunderos

echo "========================================="
echo "  Testing Program Execution from Disk"
echo "========================================="
echo ""

# Rebuild kernel and userland to ensure everything is up to date
echo "[1/5] Building kernel..."
make all > /dev/null 2>&1
echo "      ✓ Kernel built"

echo "[2/5] Building userland programs..."
make userland > /dev/null 2>&1
echo "      ✓ Userland programs built"

# Verify userland programs are valid ELF files
echo "[3/5] Verifying userland programs..."
for prog in hello cat ls; do
    if [ -f "userland/build/$prog" ]; then
        if riscv64-unknown-elf-readelf -h "userland/build/$prog" 2>&1 | grep -q "RISC-V"; then
            echo "      ✓ $prog is a valid RISC-V ELF executable"
        else
            echo "      ✗ $prog is NOT a valid RISC-V ELF"
            exit 1
        fi
    else
        echo "      ✗ $prog not found"
        exit 1
    fi
done

# Create ext2 filesystem with programs
echo "[4/5] Creating ext2 filesystem..."
rm -rf build/testfs
mkdir -p build/testfs/bin
echo "Hello from ext2 filesystem!" > build/testfs/test.txt
cp userland/build/cat build/testfs/bin/cat 2>/dev/null || true
cp userland/build/ls build/testfs/bin/ls 2>/dev/null || true  
cp userland/build/hello build/testfs/bin/hello 2>/dev/null || true
mkfs.ext2 -F -q -d build/testfs build/ext2-disk.img 10M > /dev/null 2>&1
rm -rf build/testfs
echo "      ✓ ext2 filesystem created"

# Run test with timeout
echo "[5/5] Running QEMU test..."
echo ""
echo "----------------------------------------"
echo "QEMU will start. Test these commands:"
echo "  1. ls /bin        (should show: cat, hello, ls)"
echo "  2. hello          (should print: Hello from userland!)"
echo "  3. ls /           (using external ls program)"
echo "  4. cat /test.txt  (using external cat program)"
echo ""
echo "Press Ctrl-A then X to exit QEMU"
echo "----------------------------------------"
echo ""

# Give user a moment to read instructions
sleep 2

# Start QEMU
exec qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/ext2-disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0
