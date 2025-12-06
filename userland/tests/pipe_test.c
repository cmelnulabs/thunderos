/**
 * pipe_test.c - Test program for pipe IPC
 * 
 * This program tests pipe functionality by:
 * 1. Creating a pipe
 * 2. Forking a child process
 * 3. Parent writes to pipe, child reads from pipe
 * 4. Verifying data transmission
 */

#include <stddef.h>

// Syscall numbers
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_FORK    7
#define SYS_WAIT    9
#define SYS_PIPE    26

// File descriptors
#define STDOUT_FD 1

// Helper macros for syscalls
#define syscall1(n, a1) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(syscall_number) : "memory"); \
    a0; \
})

#define syscall3(n, a1, a2, a3) ({ \
    register long a0 asm("a0") = (long)(a1); \
    register long a1_reg asm("a1") = (long)(a2); \
    register long a2_reg asm("a2") = (long)(a3); \
    register long syscall_number asm("a7") = (n); \
    asm volatile("ecall" : "+r"(a0) : "r"(a1_reg), "r"(a2_reg), "r"(syscall_number) : "memory"); \
    a0; \
})

// Syscall wrappers
static inline void exit(int status) {
    syscall1(SYS_EXIT, status);
    while(1);
}

static inline long write(int fd, const char *buf, size_t len) {
    return syscall3(SYS_WRITE, fd, buf, len);
}

static inline long read(int fd, const char *buf, size_t len) {
    return syscall3(SYS_READ, fd, buf, len);
}

static inline long pipe(const int pipefd[2]) {
    return syscall1(SYS_PIPE, pipefd);
}

static inline long fork(void) {
    return syscall1(SYS_FORK, 0);
}

static inline long waitpid(int pid, const int *wstatus, int options) {
    return syscall3(SYS_WAIT, pid, wstatus, options);
}

static inline long close(int fd) {
    // Using SYS_CLOSE (14)
    register long a0 asm("a0") = fd;
    register long syscall_number asm("a7") = 14;
    asm volatile("ecall" : "+r"(a0) : "r"(syscall_number) : "memory");
    return a0;
}

// String helpers
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    write(STDOUT_FD, s, strlen(s));
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Main test program
void _start(void) {
    int pipefd[2] = {-1, -1};
    const char *test_message = "Hello from parent through pipe!";
    char read_buffer[64];
    
    print("\n=== Pipe Test Program ===\n\n");
    
    // Test 1: Create pipe
    print("[TEST 1] Creating pipe...\n");
    if (pipe(pipefd) < 0) {
        print("[FAIL] pipe() failed\n");
        exit(1);
    }
    print("[PASS] Pipe created successfully\n");
    print("  Read FD:  ");
    // Simple number print
    char num[2];
    num[0] = (char)('0' + pipefd[0]);
    num[1] = '\0';
    print(num);
    print("\n  Write FD: ");
    num[0] = (char)('0' + pipefd[1]);
    print(num);
    print("\n\n");
    
    // Test 2: Fork and communicate
    print("[TEST 2] Forking child process...\n");
    long pid = fork();
    
    if (pid < 0) {
        print("[FAIL] fork() failed\n");
        exit(1);
    }
    
    if (pid == 0) {
        // Child process
        print("[CHILD] Child process started\n");
        
        // Close write end
        print("[CHILD] Closing write end of pipe...\n");
        close(pipefd[1]);
        
        // Read from pipe
        print("[CHILD] Reading from pipe...\n");
        long bytes_read = read(pipefd[0], read_buffer, sizeof(read_buffer) - 1);
        
        if (bytes_read < 0) {
            print("[CHILD] [FAIL] read() failed\n");
            exit(1);
        }
        
        read_buffer[bytes_read] = '\0';
        
        print("[CHILD] Read ");
        num[0] = '0' + (bytes_read / 10);
        if (num[0] != '0') print(num);
        num[0] = '0' + (bytes_read % 10);
        print(num);
        print(" bytes: \"");
        print(read_buffer);
        print("\"\n");
        
        // Verify message
        if (strcmp(read_buffer, test_message) == 0) {
            print("[CHILD] [PASS] Message matches!\n");
        } else {
            print("[CHILD] [FAIL] Message mismatch\n");
            exit(1);
        }
        
        // Close read end
        print("[CHILD] Closing read end...\n");
        close(pipefd[0]);
        
        print("[CHILD] Exiting...\n");
        exit(0);
        
    } else {
        // Parent process
        print("[PARENT] Parent process (child PID = ");
        // Print PID (simple 2-digit)
        if (pid >= 10) {
            num[0] = '0' + (pid / 10);
            print(num);
        }
        num[0] = '0' + (pid % 10);
        print(num);
        print(")\n");
        
        // Close read end
        print("[PARENT] Closing read end of pipe...\n");
        close(pipefd[0]);
        
        // Write to pipe
        print("[PARENT] Writing to pipe: \"");
        print(test_message);
        print("\"\n");
        
        long bytes_written = write(pipefd[1], test_message, strlen(test_message));
        
        if (bytes_written < 0) {
            print("[PARENT] [FAIL] write() failed\n");
            exit(1);
        }
        
        print("[PARENT] Wrote ");
        num[0] = '0' + (bytes_written / 10);
        if (num[0] != '0') print(num);
        num[0] = '0' + (bytes_written % 10);
        print(num);
        print(" bytes\n");
        
        // Close write end
        print("[PARENT] Closing write end...\n");
        close(pipefd[1]);
        
        // Wait for child
        print("[PARENT] Waiting for child to exit...\n");
        int status;
        (void)waitpid(pid, &status, 0);
        
        print("[PARENT] Child exited\n");
        print("\n[PASS] All pipe tests completed successfully!\n");
        exit(0);
    }
}
