/*
 * Absolutely minimal user shell - no helper functions
 */

extern long syscall3(long n, long a0, long a1, long a2);

void _start(void) {
    // Initialize gp
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    // Write banner directly
    const char *banner = "Minimal Shell\n";
    const char *p = banner;
    long len = 0;
    while (*p++) len++;
    syscall3(1, 1, (long)banner, len);
    
    // Write prompt
    const char *prompt = "> ";
    syscall3(1, 1, (long)prompt, 2);
    
    // Loop reading one char at a time
    while (1) {
        char c;
        long n = syscall3(2, 0, (long)&c, 1);
        
        if (n == 1) {
            // Echo it back
            syscall3(1, 1, (long)&c, 1);
            
            if (c == '\n') {
                syscall3(1, 1, (long)prompt, 2);
            }
        }
    }
}
