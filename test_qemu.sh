#!/usr/bin/env bash
#
# Build and test ThunderOS in QEMU
#
# Builds the kernel and launches it in QEMU for testing.
# Press Ctrl+A then X to exit QEMU.

set -euo pipefail

readonly KERNEL_ELF="build/thunderos.elf"
readonly QEMU="qemu-system-riscv64"

check_qemu() {
    if ! command -v "${QEMU}" &> /dev/null; then
        echo "✗ Error: ${QEMU} not found" >&2
        echo "  Install QEMU with RISC-V support" >&2
        exit 1
    fi
    if ! "${QEMU}" --version &> /dev/null; then
        echo "✗ Error: ${QEMU} is not executable or does not support RISC-V 64-bit" >&2
        echo "  Ensure QEMU is installed with RISC-V 64-bit support" >&2
        exit 1
    fi
}

build_kernel() {
    echo "Building kernel..."
    make all || { echo '✗ Build failed' >&2; exit 1; }
    
    if [[ ! -f "${KERNEL_ELF}" ]]; then
        echo "✗ Build failed" >&2
        exit 1
    fi
    
    echo "✓ Build successful"
}

run_qemu() {
    echo ""
    echo "Launching ThunderOS in QEMU..."
    echo "  (Press Ctrl+A then X to exit)"
    echo ""
    
    make qemu
}

main() {
    check_qemu
    build_kernel
    run_qemu
}

main "$@"
