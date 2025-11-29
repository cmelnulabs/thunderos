/*
 * Minimal test program - just exits
 */

void _start(void) {
    // Syscall exit(42)
    register long a7 asm("a7") = 0;  // SYS_EXIT
    register long a0 asm("a0") = 42;
    asm volatile("ecall" : : "r"(a7), "r"(a0));
    
    // Should never reach here
    while(1);
}
