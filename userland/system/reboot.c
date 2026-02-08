/**
 * reboot - Reboot the system
 * 
 * Userland utility to reboot ThunderOS.
 * Uses SYS_REBOOT syscall to request system reboot.
 */

#define SYS_WRITE   1
#define SYS_EXIT    0
#define SYS_REBOOT  201

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
 * Entry point for reboot utility
 * 
 * Initializes global pointer and requests system reboot.
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
    
    print("Rebooting ThunderOS...\n");
    syscall1(SYS_REBOOT, 0);
    
    /* Should not reach here */
    print("ERROR: Reboot failed!\n");
    syscall1(SYS_EXIT, 1);
}
