/*
 * ls - List directory contents
 * Uses getdents syscall to read directory entries
 * Supports -l for long format with permissions
 */

// ThunderOS syscall numbers
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_OPEN     13
#define SYS_CLOSE    14
#define SYS_STAT     16
#define SYS_GETDENTS 27
#define SYS_GETCWD   29

// Open flags
#define O_RDONLY    0x0000

typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// Stat structure (must match kernel vfs_stat_t)
struct stat {
    uint32_t st_ino;      // Inode number
    uint16_t st_mode;     // Permission bits and file type
    uint16_t st_uid;      // Owner user ID
    uint16_t st_gid;      // Owner group ID
    uint16_t st_pad;      // Padding for alignment
    uint32_t st_size;     // File size in bytes
    uint32_t st_type;     // VFS file type
};

// Directory entry structure (must match kernel)
struct thunderos_dirent {
    unsigned int d_ino;       // Inode number
    unsigned short d_reclen;  // Record length
    unsigned char d_type;     // File type
    char d_name[256];         // File name
};

// File type constants
#define VFS_TYPE_FILE      1
#define VFS_TYPE_DIRECTORY 2

// Permission bits (from ext2)
#define S_IRUSR  0x0100  // Owner read
#define S_IWUSR  0x0080  // Owner write
#define S_IXUSR  0x0040  // Owner execute
#define S_IRGRP  0x0020  // Group read
#define S_IWGRP  0x0010  // Group write
#define S_IXGRP  0x0008  // Group execute
#define S_IROTH  0x0004  // Other read
#define S_IWOTH  0x0002  // Other write
#define S_IXOTH  0x0001  // Other execute

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

static void print_char(char c) {
    syscall(SYS_WRITE, 1, (long)&c, 1);
}

// Print a number
static void print_num(unsigned int n) {
    char buf[16];
    int i = 0;
    
    if (n == 0) {
        print("0");
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    // Print in reverse
    while (i > 0) {
        print_char(buf[--i]);
    }
}

// Print number with padding
static void print_num_padded(unsigned int n, int width) {
    char buf[16];
    int i = 0;
    int len;
    
    if (n == 0) {
        len = 1;
    } else {
        unsigned int tmp = n;
        len = 0;
        while (tmp > 0) {
            len++;
            tmp /= 10;
        }
    }
    
    // Print leading spaces
    for (i = len; i < width; i++) {
        print_char(' ');
    }
    
    print_num(n);
}

// Print permission string like "rwxr-xr-x"
static void print_permissions(uint16_t mode, uint32_t type) {
    // File type character
    if (type == VFS_TYPE_DIRECTORY) {
        print_char('d');
    } else {
        print_char('-');
    }
    
    // Owner permissions
    print_char((mode & S_IRUSR) ? 'r' : '-');
    print_char((mode & S_IWUSR) ? 'w' : '-');
    print_char((mode & S_IXUSR) ? 'x' : '-');
    
    // Group permissions
    print_char((mode & S_IRGRP) ? 'r' : '-');
    print_char((mode & S_IWGRP) ? 'w' : '-');
    print_char((mode & S_IXGRP) ? 'x' : '-');
    
    // Other permissions
    print_char((mode & S_IROTH) ? 'r' : '-');
    print_char((mode & S_IWOTH) ? 'w' : '-');
    print_char((mode & S_IXOTH) ? 'x' : '-');
}

// Simple string concatenation for paths
static void strcat_path(char *dest, const char *dir, const char *name) {
    // Copy directory
    while (*dir) {
        *dest++ = *dir++;
    }
    // Add separator if needed
    if (dest[-1] != '/') {
        *dest++ = '/';
    }
    // Copy name
    while (*name) {
        *dest++ = *name++;
    }
    *dest = '\0';
}

// Buffer for directory entries and cwd
static char dirent_buf[4096];
static char cwd_buf[256];
static char path_buf[512];
static struct stat statbuf;

void _start(void) {
    // Always use long format to show permissions
    int long_format = 1;
    
    // Get current working directory
    char *cwd = (char *)syscall(SYS_GETCWD, (long)cwd_buf, sizeof(cwd_buf), 0);
    const char *path = (cwd != 0) ? cwd : "/";
    
    // Open the directory
    int fd = (int)syscall(SYS_OPEN, (long)path, O_RDONLY, 0);
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
            
            if (long_format) {
                // Build full path for stat
                strcat_path(path_buf, path, entry->d_name);
                
                // Get file status
                if (syscall(SYS_STAT, (long)path_buf, (long)&statbuf, 0) == 0) {
                    // Print permissions
                    print_permissions(statbuf.st_mode, statbuf.st_type);
                    print(" ");
                    
                    // Print uid:gid
                    print_num(statbuf.st_uid);
                    print(":");
                    print_num(statbuf.st_gid);
                    print(" ");
                    
                    // Print size (right-aligned, 8 chars)
                    print_num_padded(statbuf.st_size, 8);
                    print(" ");
                } else {
                    // Couldn't stat, print placeholder
                    print("----------    ?:?        ? ");
                }
                
                // Print name
                print(entry->d_name);
                print("\n");
            } else {
                if (!first) {
                    print("  ");
                }
                first = 0;
                print(entry->d_name);
            }
            
            ptr += entry->d_reclen;
        }
    }
    
    if (!long_format && !first) {
        print("\n");
    }
    
    syscall(SYS_CLOSE, fd, 0, 0);
    syscall(SYS_EXIT, 0, 0, 0);
}
