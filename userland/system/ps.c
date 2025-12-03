/*
 * ps - Process status utility
 * 
 * Display information about running processes.
 */

#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_GETPROCS 33

typedef unsigned long size_t;

/* Process info structure (must match kernel) */
#define PROC_NAME_MAX 32
typedef struct {
    int pid;
    int ppid;
    int pgid;
    int sid;
    int state;
    int tty;
    unsigned long cpu_time;
    char name[PROC_NAME_MAX];
} procinfo_t;

/* State names */
static const char *state_names[] = {
    "UNUSED",   /* 0 */
    "EMBRYO",   /* 1 */
    "SLEEP",    /* 2 */
    "READY",    /* 3 */
    "RUN",      /* 4 */
    "ZOMBIE"    /* 5 */
};

/* System call wrapper */
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

static void print_char(char c) {
    syscall3(SYS_WRITE, 1, (long)&c, 1);
}

/* Print integer with right-align in field width */
static void print_int_width(int n, int width) {
    char buf[16];
    int i = 0;
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    if (neg) buf[i++] = '-';
    
    /* Pad with spaces */
    while (i < width) {
        print_char(' ');
        width--;
    }
    
    /* Print reversed */
    while (i > 0) {
        print_char(buf[--i]);
    }
}

static void print_int(int n) {
    print_int_width(n, 1);
}

/* Print string left-aligned in field width */
static void print_str_width(const char *s, int width) {
    int len = strlen(s);
    print(s);
    while (len < width) {
        print_char(' ');
        len++;
    }
}

/* Process buffer */
static procinfo_t procs[64];

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
    
    /* Get process list */
    long count = syscall2(SYS_GETPROCS, (long)procs, 64);
    
    if (count < 0) {
        print("ps: failed to get process list\n");
        syscall2(SYS_EXIT, 1, 0);
    }
    
    /* Print header */
    print("  PID  PPID  PGID   SID TTY   STATE  TIME CMD\n");
    
    /* Print each process */
    for (int i = 0; i < count; i++) {
        procinfo_t *p = &procs[i];
        
        /* PID */
        print_int_width(p->pid, 5);
        print_char(' ');
        
        /* PPID */
        print_int_width(p->ppid, 5);
        print_char(' ');
        
        /* PGID */
        print_int_width(p->pgid, 5);
        print_char(' ');
        
        /* SID */
        print_int_width(p->sid, 5);
        print_char(' ');
        
        /* TTY */
        if (p->tty >= 0) {
            print("tty");
            print_int(p->tty + 1);
        } else {
            print("?   ");
        }
        print_char(' ');
        
        /* State */
        if (p->state >= 0 && p->state <= 5) {
            print_str_width(state_names[p->state], 7);
        } else {
            print("???    ");
        }
        
        /* CPU time (simplified - just show ticks) */
        print_int_width((int)p->cpu_time, 5);
        print_char(' ');
        
        /* Command name */
        print(p->name);
        print("\n");
    }
    
    syscall2(SYS_EXIT, 0, 0);
}
