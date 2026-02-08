# UDP Network Testing Guide

## Overview

This guide explains how to test UDP networking between ThunderOS (running in QEMU) and the Ubuntu host machine. This validates that the UDP stack works over real network interfaces, not just loopback.

## Network Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Ubuntu Host (Docker Container)                              │
│                                                              │
│  ┌──────────────────┐                                       │
│  │ test_udp_host.py │  ← Listens on 0.0.0.0:9999           │
│  │ (Python server)  │                                       │
│  └──────────────────┘                                       │
│         ↑                                                    │
│         │ UDP packets                                       │
│         │                                                    │
│  ┌──────┴───────────────────────────────────────┐          │
│  │ QEMU User-Mode Network (slirp)                │          │
│  │                                                │          │
│  │  Guest sees: 10.0.2.15 (own IP)              │          │
│  │  Host is at: 10.0.2.2 (gateway)              │          │
│  │                                                │          │
│  │  ┌──────────────────────────────────┐        │          │
│  │  │ ThunderOS Guest                  │        │          │
│  │  │                                   │        │          │
│  │  │  VirtIO-net driver               │        │          │
│  │  │  IP: 10.0.2.15                   │        │          │
│  │  │  Gateway: 10.0.2.2               │        │          │
│  │  │                                   │        │          │
│  │  │  udp_network_test program:       │        │          │
│  │  │  - Binds to port 12345           │        │          │
│  │  │  - Sends to 10.0.2.2:9999        │        │          │
│  │  │  - Receives echo responses       │        │          │
│  │  └──────────────────────────────────┘        │          │
│  └───────────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
```

## QEMU User-Mode Networking

QEMU's user-mode networking (also called SLIRP) creates a virtual network with:

- **Guest IP**: 10.0.2.15 (automatically assigned)
- **Host Gateway**: 10.0.2.2 (routes to host machine)
- **DNS**: 10.0.2.3
- **DHCP**: 10.0.2.2

When the guest sends packets to 10.0.2.2, QEMU forwards them to the host. This allows guest-to-host communication without requiring root privileges or TAP devices.

## Test Components

### 1. Host-Side UDP Echo Server

**File**: `test_udp_host.py`

Python script that:
- Listens on UDP port 9999
- Receives packets from any source
- Prints packet information (source, size, content)
- Echoes packets back to sender

**Usage**:
```bash
python3 test_udp_host.py [port]
```

Default port is 9999.

### 2. Guest-Side UDP Client

**File**: `userland/net/udp_network_test.c`

ThunderOS program that:
- Creates UDP socket
- Binds to local port 12345
- Sends 3 test packets to 10.0.2.2:9999
- Attempts to receive echo responses
- Verifies data integrity

Built as part of userland programs.

### 3. Test Launcher Script

**File**: `test_udp_network.sh`

Bash script that:
- Checks prerequisites (QEMU, kernel, filesystem)
- Prompts user to start host server
- Launches QEMU with user-mode networking
- Provides instructions

**Usage**:
```bash
./test_udp_network.sh
```

## Step-by-Step Testing

### Preparation

1. **Build ThunderOS** (if not already done):
   ```bash
   ./build_os.sh
   ```

   This builds:
   - Kernel with VirtIO-net, UDP stack
   - Userland programs including `udp_network_test`
   - ext2 filesystem image

### Host-Side Setup

2. **Start the UDP echo server**:
   ```bash
   python3 test_udp_host.py
   ```

   You should see:
   ```
   UDP Echo Server
   ===============
   Listening on 0.0.0.0:9999
   Press Ctrl+C to stop

   Waiting for UDP packets...
   ```

   Keep this terminal open.

### Guest-Side Testing

3. **Launch ThunderOS with networking**:
   
   In another terminal:
   ```bash
   ./test_udp_network.sh
   ```

   The script will:
   - Check prerequisites
   - Prompt you to confirm host server is running
   - Launch QEMU with network enabled

4. **Wait for boot**:
   
   ThunderOS will boot and show:
   ```
   [OK] VirtIO-net initialized (MAC: 52:54:00:12:34:56)
   [OK] IP configured: 10.0.2.15
   [OK] Gateway: 10.0.2.2
   ```

5. **Run the network test**:
   
   At the `ush>` prompt:
   ```
   ush> udp_network_test
   ```

6. **Observe results**:

   **In ThunderOS terminal**:
   ```
   ThunderOS UDP Network Test
   ===========================
   [TEST 1] Creating UDP socket...
   [PASS] Socket created (fd: 100)

   [TEST 2] Binding to local port 12345...
   [PASS] Bound to port 12345

   [INFO] Target host: 10.0.2.2:9999

   [TEST 3] Sending packet #1...
     Sent 23 bytes
     [PASS] Received echo: 23 bytes from 10.0.2.2
     [PASS] Data matches!

   [TEST 4] Sending packet #2...
     Sent 23 bytes
     [PASS] Received echo: 23 bytes from 10.0.2.2
     [PASS] Data matches!

   [TEST 5] Sending packet #3...
     Sent 23 bytes
     [PASS] Received echo: 23 bytes from 10.0.2.2
     [PASS] Data matches!

   =====================================
     Network Test Complete
     Packets sent: 3
     Echoes received: 3
     Status: SUCCESS - Network working!
   =====================================
   ```

   **In host server terminal**:
   ```
   [Packet 1] From 10.0.2.15:12345 - 23 bytes
   Data: ThunderOS UDP packet #1
   Echoed back to sender

   [Packet 2] From 10.0.2.15:12345 - 23 bytes
   Data: ThunderOS UDP packet #2
   Echoed back to sender

   [Packet 3] From 10.0.2.15:12345 - 23 bytes
   Data: ThunderOS UDP packet #3
   Echoed back to sender
   ```

## Troubleshooting

### No Packets Received on Host

**Symptom**: Host server shows "Waiting for UDP packets..." but nothing arrives.

**Possible causes**:

1. **VirtIO-net not initialized**
   - Check ThunderOS boot messages for `[OK] VirtIO-net initialized`
   - If missing, VirtIO driver may have failed

2. **Wrong QEMU network configuration**
   - Ensure QEMU command includes:
     ```
     -netdev user,id=net0 \
     -device virtio-net-device,netdev=net0
     ```

3. **Firewall blocking**
   - Check host firewall (unlikely in Docker)
   - Try: `sudo ufw allow 9999/udp` (if using ufw)

4. **Wrong host IP**
   - Guest should send to 10.0.2.2 (QEMU gateway)
   - This is hardcoded in `udp_network_test.c`

### Guest Reports "No echo received"

**Symptom**: Packets sent successfully but no responses.

**Possible causes**:

1. **Host server not running**
   - Verify `test_udp_host.py` is running
   - Check it shows "Listening on 0.0.0.0:9999"

2. **Wrong port**
   - Host server must be on port 9999
   - Guest sends to port 9999

3. **Receive buffer issue**
   - ThunderOS socket can only buffer 1 packet
   - If multiple packets arrive before read, data is lost
   - This is a known limitation

4. **QEMU doesn't route responses**
   - QEMU should automatically route responses
   - User-mode networking maintains connection state
   - Try restarting QEMU

### "VirtIO-net timeout" Error

**Symptom**: `[FAIL] VirtIO-net device timeout`

**Solution**: Ensure QEMU has:
```bash
-global virtio-mmio.force-legacy=false
```

This enables modern VirtIO mode required for proper operation.

### Packets Received But Data Mismatch

**Symptom**: Echo received but verification fails.

**Possible causes**:

1. **Buffer corruption**
   - Check ThunderOS kernel logs for errors
   - May indicate memory management bug

2. **Endianness issue**
   - IP addresses, ports must match byte order
   - Should be handled correctly

3. **Packet truncation**
   - Check if received size matches sent size
   - May indicate buffer size issue

## Docker Considerations

When running inside Docker container:

1. **Host IP**: 10.0.2.2 works because:
   - QEMU user-mode networking creates virtual subnet
   - 10.0.2.2 is QEMU's gateway, not Docker host
   - QEMU handles forwarding to actual host process

2. **Port binding**: Python server binds to 0.0.0.0:9999
   - Accessible from QEMU virtual network
   - No special Docker networking needed

3. **No port forwarding required**: 
   - QEMU user-mode is inside container
   - Communication stays within container
   - No need to expose ports to external host

## Alternative: Testing Without Host Server

If you can't run the host server, you can still verify that packets are being sent:

1. **Loopback test**: Use `udp_echo` instead
   ```
   ush> udp_echo
   ```
   This tests 127.0.0.1 loopback (already works).

2. **Packet capture**: Monitor network with tcpdump
   ```bash
   # In container, before running QEMU
   tcpdump -i any -n udp port 9999
   ```
   Then run ThunderOS test - you should see outgoing packets.

3. **QEMU monitor**: Check QEMU statistics
   ```
   (qemu) info network
   ```
   Shows packet counts on virtio-net device.

## Understanding the Test Results

### Success Criteria

✅ **Full success**: All 3 packets sent, all 3 echoes received, data matches
- Proves: UDP send/receive working over real network
- Validates: VirtIO-net, IP layer, UDP layer, socket API

⚠️ **Partial success**: Packets sent, but no echoes
- Proves: UDP send working, packet reaches wire
- Issue: Host server not responding or responses not routing back
- Still validates: Most of the stack works

❌ **Failure**: Socket creation or send fails
- Issue: Something wrong with socket implementation
- Debug: Check kernel logs, errno values

### What This Test Validates

1. **VirtIO-net driver**: Device initialization and packet transmission
2. **IP layer**: Packet formatting, routing, address handling
3. **UDP layer**: UDP header creation, checksum (if enabled)
4. **Socket layer**: Socket creation, binding, sendto/recvfrom
5. **Syscall layer**: Syscall interface to userland
6. **Network routing**: Packets reach host via QEMU gateway
7. **End-to-end**: Complete stack from userland to wire and back

## Next Steps

After successful network testing:

1. **Implement socket options**: SO_REUSEADDR, SO_BROADCAST, etc.
2. **Add receive queue**: Buffer multiple packets per socket
3. **Implement select/poll**: Non-blocking I/O multiplexing
4. **Add TCP**: Connection-oriented protocol
5. **Implement DNS**: Resolve hostnames to IPs
6. **Add more protocols**: ICMP echo, DHCP client, etc.

## References

- QEMU User Networking: https://wiki.qemu.org/Documentation/Networking
- VirtIO Spec: https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html
- UDP RFC: RFC 768
- Socket API: POSIX socket specification
