/*
 * clock - Display elapsed seconds since execution
 * 
 * Prints the number of seconds that have passed every second.
 * Used to test console multiplexing - run this on one VT,
 * switch to another, then switch back to see accumulated output.
 */

// ThunderOS syscall numbers
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_SLEEP   5
#define SYS_GETTIME 12

typedef unsigned long size_t;
typedef unsigned long uint64_t;

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

static void write_str(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    syscall(SYS_WRITE, 1, (long)s, len);
}

static void write_num(uint64_t n) {
    char buf[21];  /* Max 20 digits for 64-bit number + null */
    int i = 20;
    buf[i] = '\0';
    
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0) {
            buf[--i] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    write_str(&buf[i]);
}

static uint64_t gettime(void) {
    return syscall(SYS_GETTIME, 0, 0, 0);
}

static void sleep_ms(uint64_t ms) {
    syscall(SYS_SLEEP, ms, 0, 0);
}

void _start(void) {
    write_str("=== Clock Started ===\n");
    write_str("Press ESC+2 to switch to VT2, ESC+1 to come back\n\n");
    
    uint64_t start_time = gettime();
    uint64_t last_second = 0;
    uint64_t counter = 0;
    
    while (1) {
        uint64_t now = gettime();
        uint64_t elapsed_ms = now - start_time;
        uint64_t elapsed_sec = elapsed_ms / 1000;
        
        /* Only print when a new second has passed */
        if (elapsed_sec > last_second) {
            last_second = elapsed_sec;
            counter++;
            
            write_num(counter);
            write_str("\n");
        }
        
        /* Sleep a bit to avoid busy-waiting */
        sleep_ms(100);
    }
    
    /* Never reached */
    syscall(SYS_EXIT, 0, 0, 0);
    while(1);
}
