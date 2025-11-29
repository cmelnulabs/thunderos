/*
 * whoami - Print effective user name
 * 
 * ThunderOS doesn't have users yet, so this just prints "root".
 */

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_GETPID 3

typedef unsigned long size_t;

/* System call wrappers */
static inline long syscall0(long n) {
    register long num asm("a7") = n;
    register long arg0 asm("a0") = 0;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(num)
                 : "memory");
    return arg0;
}

static inline long syscall1(long n, long a0) {
    register long num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(num)
                 : "memory");
    return arg0;
}

static inline long syscall3(long n, long a0, long a1, long a2) {
    register long num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(num), "r"(arg1), "r"(arg2)
                 : "memory");
    return arg0;
}

/* Helper functions */
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    syscall3(SYS_WRITE, 1, (long)s, strlen(s));
}

void _start(void) {
    /* Initialize gp for global data access */
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    /* ThunderOS is single-user, always root */
    print("root\n");
    
    syscall1(SYS_EXIT, 0);
}
