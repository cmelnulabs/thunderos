/*
 * mkdir - Create directories
 * Uses mkdir syscall
 */

// ThunderOS syscall numbers
#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_MKDIR 17

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

// Helper functions
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    syscall(SYS_WRITE, 1, (long)s, strlen(s));
}

void _start(void) {
    // Note: In a real implementation, we would get the path from argv
    // For now, this is a demonstration that requires hardcoded path
    // or integration with the shell's argument passing
    
    print("mkdir: usage: mkdir <directory>\n");
    print("Note: mkdir requires argument passing from shell (not yet implemented)\n");
    
    syscall(SYS_EXIT, 0, 0, 0);
}
