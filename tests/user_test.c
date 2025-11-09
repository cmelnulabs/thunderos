/*
 * Simple User Mode Test Program
 * 
 * This is compiled as a standalone RISC-V program that will be loaded
 * and executed in user mode by the kernel.
 */

#include <stdint.h>

// System call numbers
#define SYS_WRITE   0
#define SYS_EXIT    1
#define SYS_GETTIME 2
#define SYS_GETPID  3

// System call helpers
static inline long syscall(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile(
        "ecall"
        : "=r" (ret)
        : "r" (n), "r" (a), "r" (b), "r" (c)
        : "memory"
    );
    return ret;
}

// Exit the program
void exit(int code) {
    syscall(SYS_EXIT, code, 0, 0);
    // Should never reach here
    while (1) {
        __asm__ volatile("wfi");
    }
}

// Get current time
uint64_t gettime(void) {
    return (uint64_t)syscall(SYS_GETTIME, 0, 0, 0);
}

// Get PID
int getpid(void) {
    return (int)syscall(SYS_GETPID, 0, 0, 0);
}

// Simple string length function
int strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

// Entry point for user program
void user_main(void) {
    // Get our PID
    int pid = getpid();
    
    // Loop a few times
    for (int i = 0; i < 3; i++) {
        // Get time
        uint64_t time = gettime();
        
        // This is a simple program - we can't actually write to UART from user mode
        // But we're testing that we execute user code successfully
        // In real user programs, syscalls would handle I/O
        
        // Just loop and yield via syscalls
        syscall(0, 0, 0, 0);  // Null syscall as a kind of yield
    }
    
    // Exit successfully
    exit(0);
}

// Minimal entry point
__attribute__((section(".text.entry")))
void _start(void) {
    user_main();
    // Should not reach here
    while (1) {
        __asm__ volatile("wfi");
    }
}
