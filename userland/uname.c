/*
 * uname - Print system information
 * 
 * Displays information about the operating system.
 * Options:
 *   -a  Print all information
 *   -s  Print kernel name (default)
 *   -n  Print network node hostname
 *   -r  Print kernel release
 *   -v  Print kernel version
 *   -m  Print machine hardware name
 */

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_UNAME 34

typedef unsigned long size_t;

/* System info structure (must match kernel) */
typedef struct {
    char sysname[64];
    char nodename[64];
    char release[64];
    char version[64];
    char machine[64];
} utsname_t;

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

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

/* utsname buffer */
static utsname_t uts;

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
    
    /* Get system info */
    if (syscall1(SYS_UNAME, (long)&uts) < 0) {
        print("uname: failed to get system info\n");
        syscall1(SYS_EXIT, 1);
    }
    
    /* For now, just print all (simulating -a) */
    /* ThunderOS doesn't have argv parsing yet, so default to -a behavior */
    
    print(uts.sysname);
    print(" ");
    print(uts.nodename);
    print(" ");
    print(uts.release);
    print(" ");
    print(uts.version);
    print(" ");
    print(uts.machine);
    print("\n");
    
    syscall1(SYS_EXIT, 0);
}
