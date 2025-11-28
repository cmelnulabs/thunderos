/*
 * Simple test for syscall wrappers
 */

#define SYS_WRITE 1
#define STDOUT 1

extern long syscall3(long n, long a0, long a1, long a2);

void _start(void) {
    const char *msg1 = "Hello 1\n";
    const char *msg2 = "Hello 2\n";
    const char *msg3 = "Hello 3\n";
    
    syscall3(SYS_WRITE, STDOUT, (long)msg1, 8);
    syscall3(SYS_WRITE, STDOUT, (long)msg2, 8);
    syscall3(SYS_WRITE, STDOUT, (long)msg3, 8);
    
    // Exit
    while (1);
}
