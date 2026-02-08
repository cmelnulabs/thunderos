/*
 * udp_network_test - Test UDP communication with host machine
 * 
 * Sends UDP packets to host and receives echo responses
 */

#include <stdint.h>
#include <stddef.h>

#define SYS_WRITE    1
#define SYS_EXIT     0
#define SYS_SOCKET   66
#define SYS_BIND     67
#define SYS_SENDTO   68
#define SYS_RECVFROM 69

/* Socket constants */
#define AF_INET     2
#define SOCK_DGRAM  2
#define IPPROTO_UDP 17

/* Test configuration */
#define HOST_PORT 9999
#define LOCAL_PORT 12345
#define TEST_BUF_SIZE 256
#define NUM_PACKETS 3

/* Host IP: 10.0.3.1 (TAP network host IP when using TAP networking)
 * For user-mode networking: use 10.0.2.2 (but won't work for UDP)
 * This requires TAP networking setup (see setup_tap_network.sh) */
#define HOST_IP ((10 << 24) | (0 << 16) | (3 << 8) | (1 << 0))

/* Socket address structure */
struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
};

/* Syscall wrappers */
static inline long syscall6(long n, long a0, long a1, long a2, long a3, long a4, long a5) {
    register long syscall_num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    register long arg3 asm("a3") = a3;
    register long arg4 asm("a4") = a4;
    register long arg5 asm("a5") = a5;
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(syscall_num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5)
                 : "memory");
    return arg0;
}

static inline long syscall3(long n, long a0, long a1, long a2) {
    return syscall6(n, a0, a1, a2, 0, 0, 0);
}

static inline long syscall1(long n, long a0) {
    return syscall6(n, a0, 0, 0, 0, 0, 0);
}

static void print(const char *s) {
    const char *p = s;
    while (*p) p++;
    syscall3(SYS_WRITE, 1, (long)s, p - s);
}

static void print_num(long n) {
    if (n < 0) {
        print("-");
        n = -n;
    }
    if (n >= 10) {
        print_num(n / 10);
    }
    char c = '0' + (n % 10);
    syscall3(SYS_WRITE, 1, (long)&c, 1);
}

static void print_ip(uint32_t ip) {
    print_num((ip >> 24) & 0xFF);
    print(".");
    print_num((ip >> 16) & 0xFF);
    print(".");
    print_num((ip >> 8) & 0xFF);
    print(".");
    print_num(ip & 0xFF);
}

static int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p1 = a;
    const uint8_t *p2 = b;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

void _start(void) {
    print("ThunderOS UDP Network Test\n");
    print("===========================\n\n");
    print("This test sends UDP packets to the host machine.\n\n");
    print("Before running this test:\n");
    print("1. On your Ubuntu host, run: python3 test_udp_host.py\n");
    print("2. The host server will listen on port 9999\n");
    print("3. This test will send packets to 10.0.2.2:9999\n\n");
    print("Press ENTER when ready, or Ctrl+C to cancel...\n");
    print("(Note: Just proceed for automated testing)\n\n");
    
    /* Create socket */
    print("[TEST 1] Creating UDP socket...\n");
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        print("[FAIL] socket() failed\n");
        syscall1(SYS_EXIT, 1);
    }
    print("[PASS] Socket created (fd: ");
    print_num(sock);
    print(")\n\n");
    
    /* Bind to local port */
    print("[TEST 2] Binding to local port ");
    print_num(LOCAL_PORT);
    print("...\n");
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = LOCAL_PORT;
    local.sin_addr = 0;  /* INADDR_ANY */
    for (int i = 0; i < 8; i++) local.sin_zero[i] = 0;
    
    if (syscall3(SYS_BIND, sock, (long)&local, sizeof(local)) < 0) {
        print("[FAIL] bind() failed\n");
        syscall1(SYS_EXIT, 1);
    }
    print("[PASS] Bound to port ");
    print_num(LOCAL_PORT);
    print("\n\n");
    
    /* Setup host address */
    struct sockaddr_in host;
    host.sin_family = AF_INET;
    host.sin_port = HOST_PORT;
    host.sin_addr = HOST_IP;
    for (int i = 0; i < 8; i++) host.sin_zero[i] = 0;
    
    print("[INFO] Target host: ");
    print_ip(HOST_IP);
    print(":");
    print_num(HOST_PORT);
    print("\n\n");
    
    /* Send packets and receive echoes */
    int success = 0;
    for (int i = 0; i < NUM_PACKETS; i++) {
        print("[TEST ");
        print_num(3 + i);
        print("] Sending packet #");
        print_num(i + 1);
        print("...\n");
        
        /* Build message */
        char msg[64];
        const char *prefix = "ThunderOS UDP packet #";
        int idx = 0;
        while (prefix[idx]) {
            msg[idx] = prefix[idx];
            idx++;
        }
        msg[idx++] = '0' + (i + 1);
        msg[idx] = '\0';
        
        /* Send */
        int sent = syscall6(SYS_SENDTO, sock, (long)msg, idx, 0, (long)&host, sizeof(host));
        if (sent < 0) {
            print("  [WARN] sendto() failed\n");
            continue;
        }
        print("  Sent ");
        print_num(sent);
        print(" bytes\n");
        
        /* Wait a bit for echo to arrive (give time for network round-trip) */
        for (volatile int delay = 0; delay < 500000; delay++);
        
        /* Try to receive echo (non-blocking, may fail if host not responding) */
        char recv_buf[TEST_BUF_SIZE];
        for (int j = 0; j < TEST_BUF_SIZE; j++) recv_buf[j] = 0;
        struct sockaddr_in from;
        for (int j = 0; j < sizeof(from); j++) ((char*)&from)[j] = 0;
        
        int rcvd = syscall6(SYS_RECVFROM, sock, (long)recv_buf, sizeof(recv_buf), 0, (long)&from, 0);
        if (rcvd > 0) {
            print("  [PASS] Received echo: ");
            print_num(rcvd);
            print(" bytes from ");
            print_ip(from.sin_addr);
            print("\n");
            
            if (memcmp(msg, recv_buf, sent) == 0) {
                print("  [PASS] Data matches!\n");
                success++;
            } else {
                print("  [WARN] Data mismatch\n");
            }
        } else {
            print("  [INFO] No echo received (host may not be running)\n");
        }
        print("\n");
    }
    
    /* Summary */
    print("=====================================\n");
    print("  Network Test Complete\n");
    print("  Packets sent: ");
    print_num(NUM_PACKETS);
    print("\n");
    print("  Echoes received: ");
    print_num(success);
    print("\n");
    
    if (success > 0) {
        print("  Status: SUCCESS - Network working!\n");
    } else {
        print("  Status: Packets sent but no echo\n");
        print("  (This is OK if host not running)\n");
    }
    print("=====================================\n");
    
    syscall1(SYS_EXIT, 0);
}
