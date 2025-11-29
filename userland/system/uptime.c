/*
 * uptime - Show system uptime
 * 
 * Displays how long the system has been running since boot.
 */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETTIME 12

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

static void print_int(unsigned long n) {
    char buf[32];
    int i = 0;
    
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

/* Print with leading zero for 2-digit numbers */
static void print_2digit(unsigned long n) {
    if (n < 10) print("0");
    print_int(n);
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
    
    /* Get time since boot in milliseconds */
    unsigned long ms = syscall0(SYS_GETTIME);
    
    /* Convert to hours, minutes, seconds */
    unsigned long total_secs = ms / 1000;
    unsigned long hours = total_secs / 3600;
    unsigned long mins = (total_secs % 3600) / 60;
    unsigned long secs = total_secs % 60;
    
    print("up ");
    
    if (hours > 0) {
        print_int(hours);
        print(" hour");
        if (hours != 1) print("s");
        print(", ");
    }
    
    if (mins > 0 || hours > 0) {
        print_int(mins);
        print(" min");
        if (mins != 1) print("s");
        print(", ");
    }
    
    print_int(secs);
    print(" sec");
    if (secs != 1) print("s");
    print("\n");
    
    /* Also print raw time */
    print("(");
    print_int(ms);
    print(" ms since boot)\n");
    
    syscall1(SYS_EXIT, 0);
}
