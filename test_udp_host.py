#!/usr/bin/env python3
"""
UDP Echo Server for testing ThunderOS networking
Run this on the Ubuntu host machine (outside Docker)

Usage:
    python3 test_udp_host.py [port]
    
Default port: 9999
"""

import socket
import sys

def udp_echo_server(port=9999):
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    
    print(f"UDP Echo Server listening on 0.0.0.0:{port}")
    print("Waiting for packets from ThunderOS...")
    print("-" * 50)
    
    packet_count = 0
    
    try:
        while True:
            # Receive data
            data, addr = sock.recvfrom(1024)
            packet_count += 1
            
            print(f"\n[Packet #{packet_count}]")
            print(f"  From: {addr[0]}:{addr[1]}")
            print(f"  Size: {len(data)} bytes")
            print(f"  Data: {data.decode('utf-8', errors='replace')}")
            
            # Echo back
            sock.sendto(data, addr)
            print(f"  Echoed back to {addr[0]}:{addr[1]}")
            
    except KeyboardInterrupt:
        print(f"\n\nReceived {packet_count} packets. Shutting down...")
    finally:
        sock.close()

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9999
    udp_echo_server(port)
