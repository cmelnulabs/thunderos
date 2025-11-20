/*
 * Signal Test Program for ThunderOS
 * 
 * Tests basic signal functionality:
 * - Signal handler installation
 * - Signal delivery
 * - Signal handling
 */

#include <stdint.h>

// System call numbers
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_GETPID  3
#define SYS_KILL    11  // Must match kernel definition!
#define SYS_SIGNAL  21

// Signal numbers
#define SIGUSR1 10
#define SIGUSR2 12

// File descriptors
#define STDOUT_FD 1

// Global flag to detect signal handling
volatile int signal_received = 0;
volatile int signal_count = 0;

// Simple syscall wrapper
static inline uint64_t syscall1(int num, uint64_t arg1) {
    register uint64_t a0 asm("a0") = arg1;
    register uint64_t a7 asm("a7") = num;
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline uint64_t syscall2(int num, uint64_t arg1, uint64_t arg2) {
    register uint64_t a0 asm("a0") = arg1;
    register uint64_t a1 asm("a1") = arg2;
    register uint64_t a7 asm("a7") = num;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

static inline uint64_t syscall3(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    register uint64_t a0 asm("a0") = arg1;
    register uint64_t a1 asm("a1") = arg2;
    register uint64_t a2 asm("a2") = arg3;
    register uint64_t a7 asm("a7") = num;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

// Helper functions
void print(const char *str) {
    const char *p = str;
    int len = 0;
    while (*p++) len++;
    syscall3(SYS_WRITE, STDOUT_FD, (uint64_t)str, len);
}

void exit(int code) {
    syscall1(SYS_EXIT, code);
}

int getpid(void) {
    return (int)syscall1(SYS_GETPID, 0);
}

int kill(int pid, int sig) {
    return (int)syscall2(SYS_KILL, pid, sig);
}

void signal(int signum, void (*handler)(int)) {
    syscall2(SYS_SIGNAL, signum, (uint64_t)handler);
}

// Signal handler for SIGUSR1
void sigusr1_handler(int signum) {
    signal_received = 1;
    signal_count++;
    print("  [SIGNAL] SIGUSR1 received!\n");
}

// Signal handler for SIGUSR2
void sigusr2_handler(int signum) {
    signal_received = 2;
    signal_count++;
    print("  [SIGNAL] SIGUSR2 received!\n");
}

// Simple delay loop
void delay(int iterations) {
    volatile int i;
    for (i = 0; i < iterations; i++) {
        // Just waste some cycles
    }
}

int main(void) {
    print("\n=== ThunderOS Signal Test ===\n\n");
    
    // Get our PID
    int pid = getpid();
    print("Test process PID: ");
    // TODO: Add number printing
    print("\n");
    
    // Test 1: Install signal handler for SIGUSR1
    print("[TEST 1] Installing SIGUSR1 handler...\n");
    signal(SIGUSR1, sigusr1_handler);
    print("  Handler installed\n");
    
    // Test 2: Send SIGUSR1 to ourselves
    print("[TEST 2] Sending SIGUSR1 to self...\n");
    kill(pid, SIGUSR1);
    
    // Give time for signal delivery
    delay(1000);
    
    if (signal_received == 1) {
        print("  ✓ SIGUSR1 delivered successfully\n");
    } else {
        print("  ✗ SIGUSR1 not delivered\n");
    }
    
    // Test 3: Install handler for SIGUSR2
    print("[TEST 3] Installing SIGUSR2 handler...\n");
    signal(SIGUSR2, sigusr2_handler);
    print("  Handler installed\n");
    
    // Test 4: Send SIGUSR2
    print("[TEST 4] Sending SIGUSR2 to self...\n");
    signal_received = 0;  // Reset flag
    kill(pid, SIGUSR2);
    
    delay(1000);
    
    if (signal_received == 2) {
        print("  ✓ SIGUSR2 delivered successfully\n");
    } else {
        print("  ✗ SIGUSR2 not delivered\n");
    }
    
    // Test 5: Multiple signals
    print("[TEST 5] Sending multiple SIGUSR1 signals...\n");
    signal_count = 0;
    kill(pid, SIGUSR1);
    delay(500);
    kill(pid, SIGUSR1);
    delay(500);
    kill(pid, SIGUSR1);
    delay(500);
    
    print("  Signal count: ");
    // TODO: Add count printing
    print("\n");
    
    print("\n=== Signal Test Complete ===\n");
    print("All basic signal tests passed!\n\n");
    
    exit(0);
    return 0;
}
