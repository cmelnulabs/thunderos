/*
 * Simple Fork Test
 */

#include <stdint.h>

#define NULL ((void *)0)
#define SYS_WRITE  1
#define SYS_EXIT   0
#define SYS_FORK   7
#define STDOUT 1

static inline long syscall0(long n) {
    register long a7 asm("a7") = n;
    register long x10 asm("a0");
    asm volatile("ecall" : "=r"(x10) : "r"(a7) : "memory");
    return x10;
}

static inline long syscall3(long n, long a0, long a1, long a2) {
    register long a7 asm("a7") = n;
    register long x10 asm("a0") = a0;
    register long x11 asm("a1") = a1;
    register long x12 asm("a2") = a2;
    asm volatile("ecall" : "+r"(x10) : "r"(a7), "r"(x11), "r"(x12) : "memory");
    return x10;
}

static void print(const char *msg) {
    const char *p = msg;
    int len = 0;
    while (*p++) len++;
    syscall3(SYS_WRITE, STDOUT, (long)msg, len);
}

void _start(void) {
    print("fork_simple_test: Before fork\n");
    
    long pid = syscall0(SYS_FORK);
    
    print("fork_simple_test: After fork, pid=");
    // TODO: print pid value
    print("\n");
    
    if (pid == 0) {
        print("fork_simple_test: I am child\n");
    } else {
        print("fork_simple_test: I am parent\n");
    }
    
    syscall3(SYS_EXIT, 0, 0, 0);
}
