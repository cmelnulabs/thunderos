/*
 * User program to test exception handling
 * This program deliberately causes exceptions to test user-mode exception handling
 */

#include <stdint.h>

// System call numbers
#define SYS_WRITE 0
#define SYS_EXIT 1

// System call wrapper
static inline uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    register uint64_t a0 __asm__("a0") = arg1;
    register uint64_t a1 __asm__("a1") = arg2;
    register uint64_t a2 __asm__("a2") = arg3;
    register uint64_t a7 __asm__("a7") = num;

    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");

    return a0;
}

// Write to console
void write(const char *str) {
    syscall(SYS_WRITE, (uint64_t)str, 0, 0);
}

// Exit program
void exit(int code) {
    syscall(SYS_EXIT, code, 0, 0);
}

void main(void) {
    write("Testing user exception handling...\n");

    // Test 1: NULL pointer dereference (should cause load page fault)
    write("Test 1: NULL pointer dereference\n");
    volatile int *ptr = (volatile int *)0;
    int value = *ptr;  // This should cause an exception

    // If we reach here, the exception wasn't handled properly
    write("ERROR: Exception not handled!\n");
    exit(1);
}