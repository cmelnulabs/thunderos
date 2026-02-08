#!/bin/bash
# Test UDP networking with host machine
# 
# This script:
# 1. Builds ThunderOS with the udp_network_test program
# 2. Launches QEMU with user-mode networking (10.0.2.15 guest IP)
# 3. In guest, 10.0.2.2 is the host gateway
#
# Before running this script:
# - On Ubuntu host: python3 /workspace/test_udp_host.py
# - The test will send packets to host IP 10.0.2.2:9999

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly BUILD_DIR="${SCRIPT_DIR}/build"
readonly KERNEL_ELF="${BUILD_DIR}/thunderos.elf"
readonly FS_IMG="${BUILD_DIR}/fs.img"

# Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  ThunderOS UDP Network Test${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${YELLOW}▶ $1${NC}"
    echo ""
}

check_prerequisites() {
    print_section "Checking prerequisites"
    
    if ! command -v qemu-system-riscv64 >/dev/null 2>&1; then
        echo -e "${RED}ERROR: qemu-system-riscv64 not found${NC}"
        exit 1
    fi
    echo "✓ QEMU found: $(which qemu-system-riscv64)"
    
    if [ ! -f "${KERNEL_ELF}" ]; then
        echo -e "${RED}ERROR: Kernel not built. Run ./build_os.sh first${NC}"
        exit 1
    fi
    echo "✓ Kernel built: ${KERNEL_ELF}"
    
    if [ ! -f "${FS_IMG}" ]; then
        echo -e "${RED}ERROR: Filesystem not found. Run ./build_os.sh first${NC}"
        exit 1
    fi
    echo "✓ Filesystem: ${FS_IMG}"
}

check_host_server() {
    print_section "Checking for host UDP server"
    
    echo "This test requires a UDP echo server on the host machine."
    echo ""
    echo -e "${YELLOW}Before continuing, in another terminal run:${NC}"
    echo -e "${GREEN}  python3 test_udp_host.py${NC}"
    echo ""
    echo "The server should show: 'UDP Echo Server listening on 0.0.0.0:9999'"
    echo ""
    echo -e "${YELLOW}Network Configuration:${NC}"
    echo "  Guest IP: 10.0.2.15 (assigned by QEMU)"
    echo "  Host IP: 10.0.2.2 (QEMU gateway, routes to host)"
    echo "  Host Server Port: 9999"
    echo ""
    
    read -p "Press ENTER when the host server is ready (or Ctrl+C to cancel)... " -r
    echo ""
}

run_qemu() {
    print_section "Launching ThunderOS with Network"
    
    echo "QEMU Configuration:"
    echo "  Machine: virt"
    echo "  RAM: 128MB"
    echo "  Network: User-mode (slirp)"
    echo "  Guest can reach host at: 10.0.2.2"
    echo ""
    echo -e "${YELLOW}Once booted:${NC}"
    echo "  1. Wait for shell prompt (ush>)"
    echo "  2. Run: udp_network_test"
    echo "  3. The test will send 3 UDP packets to 10.0.2.2:9999"
    echo "  4. Check host terminal for received packets"
    echo ""
    echo "Starting QEMU..."
    echo ""
    
    # Run QEMU with user-mode networking and UDP port forwarding
    # -netdev user: Creates a virtual network with NAT
    # -device virtio-net-device: VirtIO network device
    # Guest gets IP 10.0.2.15
    # hostfwd: Forward UDP from host 127.0.0.1:9999 to guest 10.0.2.15:12345
    #   This allows bidirectional UDP: guest sends to 10.0.2.2, reaches host
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
        -netdev user,id=net0,hostfwd=udp:127.0.0.1:9999-10.0.2.15:12345 \
        -device virtio-net-device,netdev=net0
}

main() {
    print_header
    check_prerequisites
    check_host_server
    run_qemu
}

main "$@"
