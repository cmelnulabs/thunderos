/*
 * Exec Test Program for ThunderOS
 * 
 * Tests the execve() syscall by:
 * 1. Forking a child process
 * 2. Child calls execve to replace itself with hello program
 * 3. Parent waits for child to complete
 */

#include <stdint.h>

#define NULL ((void *)0)

// System call numbers
#define SYS_WRITE  1
#define SYS_EXIT   0
#define SYS_FORK   7
#define SYS_WAIT   9
#define SYS_EXECVE 20

// File descriptors
#define STDOUT 1

// Syscall wrappers
static inline long syscall0(long n) {
    register long a7 asm("a7") = n;
    register long x10 asm("a0");
    asm volatile("ecall" : "=r"(x10) : "r"(a7) : "memory");
    return x10;
}

static inline long syscall3(long n, long a0, long a1, long a2) {
    register long a7 asm("a7") = n;
    register long x10 asm("a0") = a0;
    register long x11 asm("a1") = a1;
    register long x12 asm("a2") = a2;
    asm volatile("ecall" : "+r"(x10) : "r"(a7), "r"(x11), "r"(x12) : "memory");
    return x10;
}

static void print(const char *msg) {
    const char *p = msg;
    int len = 0;
    while (*p++) len++;
    syscall3(SYS_WRITE, STDOUT, (long)msg, len);
}

void _start(void) {
    print("exec_test: Starting fork+exec test...\n");
    
    // Fork a child process
    long pid = syscall0(SYS_FORK);
    
    if (pid < 0) {
        print("exec_test: FAILED - fork() returned error\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }
    
    if (pid == 0) {
        // Child process
        print("exec_test: Child - about to exec /bin/hello\n");
        
        // Set up argv for exec
        const char *argv[] = {
            "/bin/hello",
            NULL
        };
        
        const char *envp[] = {
            NULL
        };
        
        // Call execve - should not return
        syscall3(SYS_EXECVE, (long)"/bin/hello", (long)argv, (long)envp);
        
        // If we get here, exec failed
        print("exec_test: Child - FAILED - execve() returned\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    } else {
        // Parent process
        print("exec_test: Parent - waiting for child\n");
        
        // Wait for child
        int status;
        syscall3(SYS_WAIT, pid, (long)&status, 0);
        
        print("exec_test: Parent - child completed\n");
        print("exec_test: TEST PASSED if you saw hello output above\n");
        
        syscall3(SYS_EXIT, 0, 0, 0);
    }
    
    while (1);
}
