#!/usr/bin/env bash
#
# Build ThunderOS kernel
#
# Compiles the kernel for RISC-V 64-bit architecture.
# Output: build/thunderos.elf

set -euo pipefail

readonly KERNEL_ELF="build/thunderos.elf"

main() {
    echo "Building ThunderOS kernel..."
    
    make clean
    make all
    
    if [[ -f "${KERNEL_ELF}" ]]; then
        echo ""
        echo "✓ Kernel built successfully"
        echo "  Binary: ${KERNEL_ELF}"
    else
        echo "✗ Build failed" >&2
        exit 1
    fi
}

main "$@"
