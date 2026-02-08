#!/bin/bash
# Run ThunderOS with TAP networking for real host-guest communication

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly BUILD_DIR="${SCRIPT_DIR}/build"
readonly KERNEL_ELF="${BUILD_DIR}/thunderos.elf"
readonly FS_IMG="${BUILD_DIR}/fs.img"
readonly TAP_IFACE="tap0"

# Check if TAP interface exists
if ! ip link show ${TAP_IFACE} &>/dev/null; then
    echo "ERROR: TAP interface ${TAP_IFACE} not found"
    echo ""
    echo "Please run first:"
    echo "  sudo ./setup_tap_network.sh"
    echo ""
    exit 1
fi

echo "Launching ThunderOS with TAP networking..."
echo ""
echo "Network configuration:"
echo "  TAP interface: ${TAP_IFACE}"
echo "  Guest will be: 10.0.3.15"
echo "  Host is at: 10.0.3.1"
echo ""
echo "To test UDP:"
echo "  1. On host (another terminal): python3 test_udp_host.py"
echo "  2. In ThunderOS shell: udp_network_test"
echo ""

# Run QEMU with TAP networking
qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios none \
    -kernel "${KERNEL_ELF}" \
    -global virtio-mmio.force-legacy=false \
    -drive file="${FS_IMG}",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -netdev tap,id=net0,ifname=${TAP_IFACE},script=no,downscript=no \
    -device virtio-net-device,netdev=net0
