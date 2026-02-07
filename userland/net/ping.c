/*
 * ping - Send ICMP echo requests
 * 
 * Usage: ping <ip_address>
 *        ping 10.0.2.2  (gateway)
 */

#include <stdint.h>

/* System call numbers */
#define SYS_WRITE   1
#define SYS_EXIT    0
#define SYS_NET_PING 62   /* Network ping syscall */

/* Helper functions */
static long syscall(long num, long a0, long a1, long a2) {
    register long a7 asm("a7") = num;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    
    asm volatile ("ecall"
        : "+r"(arg0)
        : "r"(a7), "r"(arg1), "r"(arg2)
        : "memory");
    
    return arg0;
}

static void print(const char *s) {
    int len = 0;
    while (s[len]) len++;
    syscall(SYS_WRITE, 1, (long)s, len);
}

static void print_num(unsigned long n) {
    char buf[20];
    int i = 19;
    buf[i] = 0;
    
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0 && i > 0) {
            buf[--i] = '0' + (n % 10);
            n /= 10;
        }
    }
    print(&buf[i]);
}

/* Parse IP address from string (e.g., "10.0.2.2") */
static uint32_t parse_ip(const char *s) {
    uint32_t ip = 0;
    int octet = 0;
    int count = 0;
    
    while (*s && count < 4) {
        if (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (*s - '0');
        } else if (*s == '.') {
            ip = (ip << 8) | (octet & 0xFF);
            octet = 0;
            count++;
        }
        s++;
    }
    /* Add last octet */
    ip = (ip << 8) | (octet & 0xFF);
    
    return ip;
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

void _start(void) {
    /* Get command line arguments
     * Note: In a real implementation, we'd parse argc/argv from stack
     * For now, we'll use a simple approach assuming first arg after program name
     */
    
    /* Default target: QEMU gateway */
    uint32_t target = (10 << 24) | (0 << 16) | (2 << 8) | 2;  /* 10.0.2.2 */
    
    /* TODO: Parse command line arguments from stack
     * For now, just ping default gateway
     * Future: Add argc/argv parsing to support: ping <ip_address>
     */
    
    print("PING ");
    print_ip(target);
    print(" (hardcoded - DNS/args not yet supported)\n");
    
    /* Call ping syscall */
    long rtt = syscall(SYS_NET_PING, target, 0, 0);
    
    if (rtt >= 0) {
        print("Reply from ");
        print_ip(target);
        print(": time=");
        print_num(rtt);
        print("ms\n");
    } else {
        print("Request timed out\n");
    }
    
    syscall(SYS_EXIT, 0, 0, 0);
    __builtin_unreachable();
}
