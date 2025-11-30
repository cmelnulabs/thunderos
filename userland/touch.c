/*
 * touch - Create empty file or update timestamp
 */

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_OPEN  13
#define SYS_CLOSE 14

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_CREAT  0x0040

typedef unsigned long size_t;

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

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    syscall(SYS_WRITE, 1, (long)s, strlen(s));
}

/* Entry point - argc in a0, argv in a1 */
void _start(long argc, char **argv) {
    if (argc < 2) {
        print("touch: missing file operand\n");
        print("Usage: touch <filename>\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    /* Create/open the file */
    const char *filename = argv[1];
    long fd = syscall(SYS_OPEN, (long)filename, O_WRONLY | O_CREAT, 0644);
    
    if (fd < 0) {
        print("touch: cannot touch '");
        print(filename);
        print("': ");
        print("Permission denied or path error\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    /* Close immediately - file is created */
    syscall(SYS_CLOSE, fd, 0, 0);
    syscall(SYS_EXIT, 0, 0, 0);
}
