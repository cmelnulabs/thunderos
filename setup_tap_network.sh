#!/bin/bash
# Setup TAP networking for ThunderOS real network testing
# This allows guest-host UDP communication

set -e

readonly TAP_IFACE="tap0"
readonly TAP_IP="10.0.3.1"
readonly GUEST_IP="10.0.3.15"
readonly NETMASK="255.255.255.0"

echo "Setting up TAP networking for ThunderOS..."
echo ""
echo "This script requires root privileges to:"
echo "  - Create TAP network interface"
echo "  - Configure IP addresses"
echo "  - Enable IP forwarding"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "ERROR: This script must be run as root"
    echo "Usage: sudo $0"
    exit 1
fi

# Create TAP interface
echo "[1/4] Creating TAP interface ${TAP_IFACE}..."
ip tuntap add dev ${TAP_IFACE} mode tap user $(logname) 2>/dev/null || true
ip link set ${TAP_IFACE} up

# Configure TAP interface IP
echo "[2/4] Configuring IP ${TAP_IP} on ${TAP_IFACE}..."
ip addr add ${TAP_IP}/24 dev ${TAP_IFACE} 2>/dev/null || true

# Enable IP forwarding
echo "[3/4] Enabling IP forwarding..."
sysctl -w net.ipv4.ip_forward=1 > /dev/null

# Setup iptables for NAT (optional, for internet access from guest)
echo "[4/4] Configuring NAT..."
iptables -t nat -A POSTROUTING -s 10.0.3.0/24 -j MASQUERADE 2>/dev/null || true
iptables -A FORWARD -i ${TAP_IFACE} -j ACCEPT 2>/dev/null || true
iptables -A FORWARD -o ${TAP_IFACE} -j ACCEPT 2>/dev/null || true

echo ""
echo "✓ TAP networking configured!"
echo ""
echo "Network configuration:"
echo "  Host TAP interface: ${TAP_IFACE}"
echo "  Host IP: ${TAP_IP}"
echo "  Guest IP: ${GUEST_IP} (configure in ThunderOS)"
echo "  Netmask: ${NETMASK}"
echo ""
echo "To test UDP:"
echo "  1. On host: python3 test_udp_host.py"
echo "  2. Start ThunderOS with: ./run_with_tap.sh"
echo "  3. In ThunderOS: udp_network_test"
echo ""
echo "To clean up: sudo $0 cleanup"
