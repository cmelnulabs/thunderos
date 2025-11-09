/*
 * User Space Hello World Program
 * 
 * This program demonstrates user-space syscalls:
 * - SYS_WRITE to output to console
 * - SYS_GETPID to get current process ID
 * - SYS_YIELD to voluntarily yield CPU
 * - SYS_EXIT to terminate cleanly
 * 
 * Compiled as a standalone RISC-V user-space program and loaded by the kernel.
 */

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// System Call Numbers (must match kernel/include/kernel/syscall.h)
// ============================================================================
#define SYS_EXIT        0   // Exit process
#define SYS_WRITE       1   // Write to file descriptor
#define SYS_READ        2   // Read from file descriptor
#define SYS_GETPID      3   // Get process ID
#define SYS_SBRK        4   // Adjust heap size
#define SYS_SLEEP       5   // Sleep for milliseconds
#define SYS_YIELD       6   // Yield CPU to scheduler
#define SYS_FORK        7   // Fork process (future)
#define SYS_EXEC        8   // Execute program (future)
#define SYS_WAIT        9   // Wait for child (future)
#define SYS_GETPPID     10  // Get parent process ID
#define SYS_KILL        11  // Send signal to process
#define SYS_GETTIME     12  // Get system time (milliseconds since boot)

// Standard file descriptors
#define STDIN_FD        0
#define STDOUT_FD       1
#define STDERR_FD       2

// ============================================================================
// Low-level Syscall Interface
// ============================================================================

/**
 * Perform a system call
 * 
 * Inline assembly to invoke ECALL instruction with 6 argument support.
 * The syscall number should be in the 'n' register (a7/x17).
 * 
 * @param n Syscall number (loaded into a7)
 * @param a First argument (a0)
 * @param b Second argument (a1)
 * @param c Third argument (a2)
 * @param d Fourth argument (a3)
 * @param e Fifth argument (a4)
 * @param f Sixth argument (a5)
 * @return Return value from syscall (in a0)
 */
static inline long syscall(long n, long a, long b, long c, long d, long e, long f) {
    register long result asm("a0") = a;
    register long arg1 asm("a1") = b;
    register long arg2 asm("a2") = c;
    register long arg3 asm("a3") = d;
    register long arg4 asm("a4") = e;
    register long arg5 asm("a5") = f;
    register long syscall_num asm("a7") = n;
    
    __asm__ volatile(
        "ecall"
        : "+r" (result)
        : "r" (arg1), "r" (arg2), "r" (arg3), "r" (arg4), "r" (arg5), "r" (syscall_num)
        : "memory"
    );
    
    return result;
}

// Simpler version for when we don't need all 6 args
static inline long syscall3(long n, long a, long b, long c) {
    return syscall(n, a, b, c, 0, 0, 0);
}

static inline long syscall1(long n, long a) {
    return syscall(n, a, 0, 0, 0, 0, 0);
}

static inline long syscall0(long n) {
    return syscall(n, 0, 0, 0, 0, 0, 0);
}

// ============================================================================
// User Space Library Functions
// ============================================================================

/**
 * Get length of null-terminated string
 */
static inline int strlen(const char *str) {
    int len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

/**
 * Exit the current process
 * 
 * This function never returns - it terminates the process with the given
 * exit code.
 */
void exit(int exit_code) {
    syscall1(SYS_EXIT, exit_code);
    // Should never reach here, but include infinite loop for safety
    while (1) {
        __asm__ volatile("wfi");
    }
}

/**
 * Write data to a file descriptor
 * 
 * @param fd File descriptor (1 = stdout, 2 = stderr)
 * @param buf Data to write
 * @param count Number of bytes
 * @return Number of bytes written, or -1 on error
 */
int write(int fd, const char *buf, size_t count) {
    return (int)syscall3(SYS_WRITE, fd, (long)buf, count);
}

/**
 * Convenience function: write string to stdout
 */
void print_string(const char *str) {
    write(STDOUT_FD, str, strlen(str));
}

/**
 * Print a single character
 */
void print_char(char c) {
    write(STDOUT_FD, &c, 1);
}

/**
 * Print an integer in decimal
 * 
 * @param num Number to print
 */
void print_int(int num) {
    if (num == 0) {
        print_char('0');
        return;
    }
    
    // Handle negative numbers
    if (num < 0) {
        print_char('-');
        num = -num;
    }
    
    // Print digits in reverse order (we'll build a string)
    char buffer[20];
    int len = 0;
    int temp = num;
    
    while (temp > 0) {
        buffer[len] = '0' + (temp % 10);
        temp /= 10;
        len++;
    }
    
    // Print in correct order
    for (int i = len - 1; i >= 0; i--) {
        print_char(buffer[i]);
    }
}

/**
 * Get current process ID
 * 
 * @return Current PID
 */
int getpid(void) {
    return (int)syscall0(SYS_GETPID);
}

/**
 * Get parent process ID
 * 
 * @return Parent PID
 */
int getppid(void) {
    return (int)syscall0(SYS_GETPPID);
}

/**
 * Get current system time in milliseconds since boot
 * 
 * @return System time in milliseconds
 */
uint64_t gettime(void) {
    return (uint64_t)syscall0(SYS_GETTIME);
}

/**
 * Voluntarily yield CPU to scheduler
 * 
 * Allows other processes to run without waiting for time slice to expire.
 */
void yield(void) {
    syscall0(SYS_YIELD);
}

/**
 * Sleep for specified number of milliseconds
 * 
 * @param milliseconds Time to sleep
 * @return 0 on success
 */
int sleep(uint64_t milliseconds) {
    return (int)syscall1(SYS_SLEEP, milliseconds);
}

// ============================================================================
// User Program Entry Point
// ============================================================================

/**
 * Main user program
 * 
 * Demonstrates basic syscalls and user-space functionality.
 */
void user_main(void) {
    // Header
    print_string("=================================\n");
    print_string("ThunderOS User Space Hello World\n");
    print_string("=================================\n\n");
    
    // Get and display process IDs
    print_string("Process Information:\n");
    print_string("  Current PID:  ");
    print_int(getpid());
    print_string("\n");
    
    print_string("  Parent PID:   ");
    print_int(getppid());
    print_string("\n");
    
    print_string("  System time:  ");
    print_int((int)(gettime() / 1000));
    print_string("s\n\n");
    
    // Demonstrate multiple prints
    print_string("Output Test (5 lines):\n");
    for (int i = 1; i <= 5; i++) {
        print_string("  Line ");
        print_int(i);
        print_string("\n");
    }
    
    print_string("\n");
    
    // Demonstrate yielding
    print_string("Yielding to scheduler...\n");
    for (int i = 0; i < 3; i++) {
        yield();
    }
    
    print_string("\nUser program exiting successfully!\n");
    print_string("=================================\n");
    
    // Exit cleanly
    exit(0);
}

// ============================================================================
// Program Entry Point (called by kernel)
// ============================================================================

/**
 * Entry point for user program
 * 
 * The kernel loads this at a fixed address and jumps here to start user code.
 * This is the first function executed in user mode.
 */
__attribute__((section(".text.entry")))
void _start(void) {
    // Call main user program
    user_main();
    
    // Should never reach here (exit is noreturn), but provide safety net
    while (1) {
        __asm__ volatile("wfi");
    }
}
