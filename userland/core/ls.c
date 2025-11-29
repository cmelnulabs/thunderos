/*
 * ls - List directory contents
 * Uses getdents syscall to read directory entries
 */

// ThunderOS syscall numbers
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_OPEN     13
#define SYS_CLOSE    14
#define SYS_GETDENTS 27
#define SYS_GETCWD   29

// Open flags
#define O_RDONLY    0x0000

typedef unsigned long size_t;
typedef long ssize_t;

// Directory entry structure (must match kernel)
struct thunderos_dirent {
    unsigned int d_ino;       // Inode number
    unsigned short d_reclen;  // Record length
    unsigned char d_type;     // File type
    char d_name[256];         // File name
};

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

// Buffer for directory entries and cwd
static char dirent_buf[4096];
static char cwd_buf[256];

void _start(void) {
    // Get current working directory
    char *cwd = (char *)syscall(SYS_GETCWD, (long)cwd_buf, sizeof(cwd_buf), 0);
    const char *path = (cwd != 0) ? cwd : "/";
    
    // Open the directory
    int fd = syscall(SYS_OPEN, (long)path, O_RDONLY, 0);
    if (fd < 0) {
        print("ls: cannot access '");
        print(path);
        print("': No such file or directory\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    // Read directory entries
    ssize_t nread;
    int first = 1;
    
    while ((nread = syscall(SYS_GETDENTS, fd, (long)dirent_buf, sizeof(dirent_buf))) > 0) {
        // Process each entry
        char *ptr = dirent_buf;
        char *end = dirent_buf + nread;
        
        while (ptr < end) {
            struct thunderos_dirent *entry = (struct thunderos_dirent *)ptr;
            
            // Skip . and .. entries
            if (entry->d_name[0] == '.' && 
                (entry->d_name[1] == '\0' || 
                 (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
                ptr += entry->d_reclen;
                continue;
            }
            
            if (!first) {
                print("  ");
            }
            first = 0;
            
            print(entry->d_name);
            
            ptr += entry->d_reclen;
        }
    }
    
    if (!first) {
        print("\n");
    }
    
    syscall(SYS_CLOSE, fd, 0, 0);
    syscall(SYS_EXIT, 0, 0, 0);
}
