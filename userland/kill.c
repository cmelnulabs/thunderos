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

/* Entry point - argc in a0, argv in a1 */
void _start(long argc, char **argv) {
    if (argc < 2) {
        print("kill: missing operand\n");
        print("Usage: kill <pid>\n");
        syscall1(SYS_EXIT, 1);
    }
    
    int pid = atoi(argv[1]);
    if (pid <= 0) {
        print("kill: invalid pid\n");
        syscall1(SYS_EXIT, 1);
    }
    
    long result = syscall2(SYS_KILL, pid, SIGTERM);
    if (result < 0) {
        print("kill: (");
        /* Print pid */
        char buf[16];
        int i = 0;
        int n = pid;
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
        while (i > 0) {
            char c = buf[--i];
            syscall3(SYS_WRITE, 1, (long)&c, 1);
        }
        print(") - No such process\n");
        syscall1(SYS_EXIT, 1);
    }
    
    syscall1(SYS_EXIT, 0);
}
