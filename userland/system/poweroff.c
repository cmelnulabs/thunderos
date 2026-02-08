/**
 * poweroff - Shutdown the system
 * 
 * Userland utility to gracefully power off ThunderOS.
 * Uses SYS_POWEROFF syscall to request system shutdown.
 */

#define SYS_WRITE   1
#define SYS_EXIT    0
#define SYS_POWEROFF 200

long syscall1(long n, long a0) {
    register long a7 asm("a7") = n;
    register long x10 asm("a0") = a0;
    asm volatile("ecall" : "+r"(x10) : "r"(a7) : "memory");
    return x10;
}

void print(const char *s) {
    syscall1(SYS_WRITE, (long)s);
}

/**
 * Entry point for poweroff utility
 * 
 * Initializes global pointer and requests system shutdown.
 */
void _start(void) {
    /* Initialize global pointer for RISC-V */
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    print("Shutting down ThunderOS...\n");
    syscall1(SYS_POWEROFF, 0);
    
    /* Should not reach here */
    print("ERROR: Poweroff failed!\n");
    syscall1(SYS_EXIT, 1);
}
