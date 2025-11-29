/*
 * sleep - Sleep for specified seconds
 */

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_SLEEP 5

typedef unsigned long size_t;

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

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    syscall(SYS_WRITE, 1, (long)s, strlen(s));
}

void _start(void) {
    // Note: Requires argument passing from shell
    // For now, just show usage
    print("sleep: usage: sleep <seconds>\n");
    print("Note: sleep requires argument passing (not yet implemented)\n");
    syscall(SYS_EXIT, 0, 0, 0);
}
