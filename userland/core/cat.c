/*
 * cat - Concatenate files and print to stdout
 * 
 * Usage: cat [file...]
 * If no files specified, reads from stdin (for pipe support)
 */

// ThunderOS syscall numbers
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_OPEN 13
#define SYS_CLOSE 14

#define STDIN_FD 0
#define STDOUT_FD 1
#define O_RDONLY 0
#define READ_BUFFER_SIZE 512

typedef unsigned long size_t;
typedef long ssize_t;

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

static void print(const char *s) {
    syscall(SYS_WRITE, STDOUT_FD, (long)s, strlen(s));
}

// Read from fd and write to stdout
static void cat_fd(int fd) {
    char buf[READ_BUFFER_SIZE];
    
    while (1) {
        long nread = syscall(SYS_READ, fd, (long)buf, sizeof(buf));
        if (nread <= 0) break;
        
        syscall(SYS_WRITE, STDOUT_FD, (long)buf, nread);
    }
}

// Read file and write to stdout
static int cat_file(const char *filename) {
    int fd = syscall(SYS_OPEN, (long)filename, O_RDONLY, 0);
    if (fd < 0) {
        print("cat: ");
        print(filename);
        print(": No such file or directory\n");
        return 1;
    }
    
    cat_fd(fd);
    syscall(SYS_CLOSE, fd, 0, 0);
    return 0;
}

void _start(int argc, char **argv) {
    /* Initialize global pointer for RISC-V */
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    int exit_code = 0;
    
    if (argc <= 1) {
        /* No arguments - read from stdin (for pipes) */
        cat_fd(STDIN_FD);
    } else {
        /* Cat each file */
        for (int i = 1; i < argc; i++) {
            if (cat_file(argv[i]) != 0) {
                exit_code = 1;
            }
        }
    }
    
    syscall(SYS_EXIT, exit_code, 0, 0);
}
