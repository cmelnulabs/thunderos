/*
 * kill - Send signal to a process
 * 
 * Usage: kill <pid>
 * 
 * Sends SIGTERM (15) to the specified process.
 */

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_KILL  11

#define SIGTERM 15

typedef unsigned long size_t;

/* System call wrappers */
static inline long syscall1(long n, long a0) {
    register long num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(num)
                 : "memory");
    return arg0;
}

static inline long syscall2(long n, long a0, long a1) {
    register long num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(num), "r"(arg1)
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

static void print_int(int n) {
    char buf[16];
    int i = 0;
    
    if (n < 0) {
        print("-");
        n = -n;
    }
    
    if (n == 0) {
        print("0");
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    while (i > 0) {
        char c = buf[--i];
        syscall3(SYS_WRITE, 1, (long)&c, 1);
    }
}

/* Parse integer from string */
static int atoi(const char *s) {
    int n = 0;
    int neg = 0;
    
    if (*s == '-') {
        neg = 1;
        s++;
    }
    
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    
    return neg ? -n : n;
}

/* Arguments passed from shell (simplified) */
extern char __arg1[];  /* First argument (PID) */

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
    
    /* For now, show usage since we don't have proper argv */
    print("kill: usage: kill <pid>\n");
    print("Note: argument parsing not yet implemented\n");
    
    syscall1(SYS_EXIT, 0);
}
