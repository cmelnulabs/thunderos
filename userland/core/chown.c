/*
 * chown - Change file owner and group
 * Usage: chown <owner>[:<group>] <file>
 * 
 * Also provides chgrp functionality via: chown :<group> <file>
 */

// ThunderOS syscall numbers
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_STAT     16
#define SYS_CHOWN    42

typedef unsigned long size_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// Stat structure (must match kernel vfs_stat_t)
struct stat {
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_uid;
    uint16_t st_gid;
    uint16_t st_pad;
    uint32_t st_size;
    uint32_t st_type;
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

// Check if character is a digit
static int isdigit(char c) {
    return c >= '0' && c <= '9';
}

// Parse a number from string
static int parse_number(const char *str, const char **end, uint16_t *value) {
    if (!isdigit(*str)) {
        return -1;
    }
    
    uint16_t result = 0;
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    *value = result;
    *end = str;
    return 0;
}

// Parse owner:group string
// Returns: 0 on success, -1 on error
// If owner is not specified (e.g., ":group"), uid is set to (uint16_t)-1
// If group is not specified (e.g., "owner"), gid is set to (uint16_t)-1
static int parse_owner_group(const char *str, uint16_t *uid, uint16_t *gid) {
    *uid = (uint16_t)-1;
    *gid = (uint16_t)-1;
    
    const char *p = str;
    const char *end;
    
    // Check for :group format (chgrp style)
    if (*p == ':') {
        p++;
        if (*p == '\0') {
            return -1;  // Just ":" is invalid
        }
        if (parse_number(p, &end, gid) < 0) {
            return -1;
        }
        if (*end != '\0') {
            return -1;  // Extra characters after group
        }
        return 0;
    }
    
    // Parse owner
    if (parse_number(p, &end, uid) < 0) {
        return -1;
    }
    
    // Check for optional :group
    if (*end == ':') {
        end++;
        if (*end != '\0') {
            if (parse_number(end, &end, gid) < 0) {
                return -1;
            }
        }
        // else: "owner:" means just change owner
    }
    
    if (*end != '\0') {
        return -1;  // Extra characters
    }
    
    return 0;
}

static struct stat statbuf;

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
    
    if (argc < 3) {
        print("Usage: chown <owner>[:<group>] <file>\n");
        print("       chown :<group> <file>        (change group only)\n");
        print("  owner/group: numeric IDs (e.g., 0, 1000)\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    const char *owner_str = argv[1];
    const char *path = argv[2];
    uint16_t new_uid;
    uint16_t new_gid;
    
    // Parse owner:group
    if (parse_owner_group(owner_str, &new_uid, &new_gid) < 0) {
        print("chown: invalid owner/group '");
        print(owner_str);
        print("'\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    // If either uid or gid is -1, we need to get the current value
    if (new_uid == (uint16_t)-1 || new_gid == (uint16_t)-1) {
        if (syscall(SYS_STAT, (long)path, (long)&statbuf, 0) != 0) {
            print("chown: cannot stat '");
            print(path);
            print("'\n");
            syscall(SYS_EXIT, 1, 0, 0);
        }
        
        if (new_uid == (uint16_t)-1) {
            new_uid = statbuf.st_uid;
        }
        if (new_gid == (uint16_t)-1) {
            new_gid = statbuf.st_gid;
        }
    }
    
    // Call chown syscall
    long result = syscall(SYS_CHOWN, (long)path, new_uid, new_gid);
    
    if (result != 0) {
        print("chown: cannot change ownership of '");
        print(path);
        print("'\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    syscall(SYS_EXIT, 0, 0, 0);
}
