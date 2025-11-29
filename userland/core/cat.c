/*
 * cat - Concatenate files and print to stdout
 * Simple implementation using open/read/write syscalls
 */

// ThunderOS syscall numbers
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_OPEN 13
#define SYS_CLOSE 14

#define AT_FDCWD -100
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
    syscall(SYS_WRITE, 1, (long)s, strlen(s));
}

void _start(void) {
    char buf[READ_BUFFER_SIZE];
    
    // For now, cat just reads test.txt as a demo
    const char *filename = "test.txt";
    
    int fd = syscall(SYS_OPEN, (long)filename, O_RDONLY, 0);
    if (fd < 0) {
        print("cat: ");
        print(filename);
        print(": No such file or directory\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    // Read and write file contents
    while (1) {
        long nread = syscall(SYS_READ, fd, (long)buf, sizeof(buf));
        if (nread <= 0) break;
        
        syscall(SYS_WRITE, 1, (long)buf, nread);
    }
    
    syscall(SYS_CLOSE, fd, 0, 0);
    syscall(SYS_EXIT, 0, 0, 0);
}
