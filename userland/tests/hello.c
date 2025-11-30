/*
 * hello - Simple hello world test
 */

// ThunderOS syscall numbers
#define SYS_EXIT 0
#define SYS_WRITE 1

typedef unsigned long size_t;

// System call wrapper
static inline long syscall(long n, long a0, long a1, long a2) {
    register long syscall_num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(syscall_num), "r"(arg1), "r"(arg2)
                 : "memory");
    
    return arg0;
}

void _start(void) {
    const char *msg = "Hello from userland!\n";
    
    // Calculate length
    size_t len = 0;
    while (msg[len]) len++;
    
    // Write message
    syscall(SYS_WRITE, 1, (long)msg, len);
    
    // Exit
    syscall(SYS_EXIT, 0, 0, 0);
    
    // Should never reach here
    while(1);
}
