/*
 * fork_test - Test fork() system call
 * 
 * Creates a child process and verifies:
 * - Parent receives child PID
 * - Child receives 0
 * - Both processes can execute independently
 */

// ThunderOS syscall numbers
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  3
#define SYS_FORK    7

typedef unsigned long size_t;
typedef int pid_t;

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

// Helper functions
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void write_str(const char *str) {
    syscall(SYS_WRITE, 1, (long)str, strlen(str));
}

static void write_num(int num) {
    char buf[20];
    int i = 0;
    int is_neg = 0;
    
    if (num == 0) {
        write_str("0");
        return;
    }
    
    if (num < 0) {
        is_neg = 1;
        num = -num;
    }
    
    // Convert to string (reversed)
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    // Add negative sign if needed
    if (is_neg) {
        buf[i++] = '-';
    }
    
    // Reverse and print
    char output[20];
    for (int j = 0; j < i; j++) {
        output[j] = buf[i - 1 - j];
    }
    output[i] = '\0';
    
    syscall(SYS_WRITE, 1, (long)output, i);
}

void _start(void) {
    write_str("=== Fork Test ===\n");
    
    // Get parent PID before fork
    pid_t parent_pid = syscall(SYS_GETPID, 0, 0, 0);
    write_str("Parent PID: ");
    write_num(parent_pid);
    write_str("\n");
    
    // Fork
    write_str("Calling fork()...\n");
    pid_t fork_result = syscall(SYS_FORK, 0, 0, 0);
    
    if (fork_result < 0) {
        // Fork failed
        write_str("[FAIL] fork() returned error\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    if (fork_result == 0) {
        // Child process
        write_str("[CHILD] fork() returned 0 - I am the child!\n");
        
        pid_t my_pid = syscall(SYS_GETPID, 0, 0, 0);
        write_str("[CHILD] My PID: ");
        write_num(my_pid);
        write_str("\n");
        
        if (my_pid == parent_pid) {
            write_str("[FAIL] Child has same PID as parent!\n");
            syscall(SYS_EXIT, 1, 0, 0);
        }
        
        write_str("[CHILD] Exiting...\n");
        syscall(SYS_EXIT, 0, 0, 0);
    } else {
        // Parent process
        write_str("[PARENT] fork() returned child PID: ");
        write_num(fork_result);
        write_str("\n");
        
        pid_t my_pid = syscall(SYS_GETPID, 0, 0, 0);
        write_str("[PARENT] My PID: ");
        write_num(my_pid);
        write_str("\n");
        
        if (my_pid != parent_pid) {
            write_str("[FAIL] Parent PID changed after fork!\n");
            syscall(SYS_EXIT, 1, 0, 0);
        }
        
        write_str("[PARENT] Test PASSED!\n");
        syscall(SYS_EXIT, 0, 0, 0);
    }
    
    // Should never reach here
    while(1);
}
