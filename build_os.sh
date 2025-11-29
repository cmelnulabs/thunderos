#!/usr/bin/env bash
#
# Build ThunderOS kernel and filesystem
#
# Compiles the kernel for RISC-V 64-bit architecture and creates
# an ext2 filesystem image for testing.
# Output: build/thunderos.elf, build/fs.img

set -euo pipefail

readonly KERNEL_ELF="build/thunderos.elf"
readonly FS_IMG="build/fs.img"
readonly FS_SIZE="10M"

main() {
    echo "Building ThunderOS kernel..."
    
    make clean
    make all
    
    if [[ ! -f "${KERNEL_ELF}" ]]; then
        echo "✗ Build failed" >&2
        exit 1
    fi
    
    echo ""
    echo "✓ Kernel built successfully"
    echo "  Binary: ${KERNEL_ELF}"
    
    # Create ext2 filesystem image
    echo ""
    echo "Creating ext2 filesystem image (${FS_SIZE})..."
    
    # Create temporary directory for filesystem contents
    rm -rf build/testfs
    mkdir -p build/testfs/bin
    
    # Add test files
    echo "Hello from ThunderOS ext2 filesystem!" > build/testfs/test.txt
    echo "This is a sample file for testing." > build/testfs/README.txt
    
    # Build userland programs if available
    if [[ -f "build_userland.sh" ]]; then
        echo "Building userland programs..."
        chmod +x build_userland.sh
        ./build_userland.sh 2>/dev/null || echo "⚠ Userland build skipped"
        
        # Copy userland binaries if they exist
        for prog in cat ls hello clock pwd mkdir rmdir touch rm clear sleep ush; do
            if [[ -f "userland/build/${prog}" ]]; then
                cp "userland/build/${prog}" build/testfs/bin/
                echo "  Added /bin/${prog}"
            fi
        done
    fi
    
    # Create ext2 filesystem
    if command -v mkfs.ext2 &> /dev/null; then
        mkfs.ext2 -F -q -d build/testfs "${FS_IMG}" "${FS_SIZE}"
        rm -rf build/testfs
        echo "✓ Filesystem created: ${FS_IMG}"
        echo "  Size: $(du -h "${FS_IMG}" | cut -f1)"
    else
        echo "⚠ mkfs.ext2 not found, skipping filesystem creation"
        echo "  Install e2fsprogs: sudo apt-get install e2fsprogs"
    fi
    
    echo ""
    echo "Build complete! Run with:"
    echo "  qemu-system-riscv64 -M virt -m 128M -nographic -serial mon:stdio \\"
    echo "    -bios none -kernel ${KERNEL_ELF} \\"
    echo "    -drive file=${FS_IMG},if=none,format=raw,id=hd0 \\"
    echo "    -device virtio-blk-device,drive=hd0 \\"
    echo "    -global virtio-mmio.force-legacy=false"
}

main "$@"
