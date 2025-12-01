/*
 * chmod - Change file permissions
 * Usage: chmod <mode> <file>
 * Mode can be octal (e.g., 755) or symbolic (e.g., u+x)
 */

// ThunderOS syscall numbers
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_CHMOD    41

typedef unsigned long size_t;
typedef unsigned int uint32_t;

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

// Check if character is an octal digit
static int isoctal(char c) {
    return c >= '0' && c <= '7';
}

// Parse octal mode string (e.g., "755" -> 0755)
static int parse_octal_mode(const char *str, uint32_t *mode) {
    uint32_t result = 0;
    
    while (*str) {
        if (!isoctal(*str)) {
            return -1;  // Invalid octal digit
        }
        result = (result << 3) | (*str - '0');
        str++;
    }
    
    *mode = result;
    return 0;
}

// Permission bit constants
#define S_IRUSR  0x0100  // Owner read
#define S_IWUSR  0x0080  // Owner write
#define S_IXUSR  0x0040  // Owner execute
#define S_IRGRP  0x0020  // Group read
#define S_IWGRP  0x0010  // Group write
#define S_IXGRP  0x0008  // Group execute
#define S_IROTH  0x0004  // Other read
#define S_IWOTH  0x0002  // Other write
#define S_IXOTH  0x0001  // Other execute

// Parse symbolic mode (e.g., "u+x", "go-w", "a+r")
// Returns -1 on error, 0 on success
// For simplicity, only supports: [ugoa][+-][rwx]+
static int parse_symbolic_mode(const char *str, uint32_t current_mode, uint32_t *new_mode) {
    uint32_t who_mask = 0;
    int op = 0;  // '+' or '-'
    uint32_t perm_bits = 0;
    
    const char *p = str;
    
    // Parse who (u, g, o, a)
    while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
        if (*p == 'u') {
            who_mask |= (S_IRUSR | S_IWUSR | S_IXUSR);
        } else if (*p == 'g') {
            who_mask |= (S_IRGRP | S_IWGRP | S_IXGRP);
        } else if (*p == 'o') {
            who_mask |= (S_IROTH | S_IWOTH | S_IXOTH);
        } else if (*p == 'a') {
            who_mask = 0x1FF;  // All bits
        }
        p++;
    }
    
    // Default to 'a' if no who specified
    if (who_mask == 0) {
        who_mask = 0x1FF;
    }
    
    // Parse operator (+ or -)
    if (*p == '+') {
        op = '+';
    } else if (*p == '-') {
        op = '-';
    } else if (*p == '=') {
        op = '=';
    } else {
        return -1;  // Invalid operator
    }
    p++;
    
    // Parse permissions (r, w, x)
    while (*p) {
        if (*p == 'r') {
            perm_bits |= (S_IRUSR | S_IRGRP | S_IROTH);
        } else if (*p == 'w') {
            perm_bits |= (S_IWUSR | S_IWGRP | S_IWOTH);
        } else if (*p == 'x') {
            perm_bits |= (S_IXUSR | S_IXGRP | S_IXOTH);
        } else {
            return -1;  // Invalid permission
        }
        p++;
    }
    
    // Apply the mask to get only the relevant bits
    perm_bits &= who_mask;
    
    // Calculate new mode
    if (op == '+') {
        *new_mode = current_mode | perm_bits;
    } else if (op == '-') {
        *new_mode = current_mode & ~perm_bits;
    } else {  // op == '='
        *new_mode = (current_mode & ~who_mask) | perm_bits;
    }
    
    return 0;
}

void _start(int argc, char **argv) {
    if (argc < 3) {
        print("Usage: chmod <mode> <file>\n");
        print("  mode: octal (e.g., 755) or symbolic (e.g., u+x, go-w)\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    const char *mode_str = argv[1];
    const char *path = argv[2];
    uint32_t mode;
    
    // Try parsing as octal first
    if (isdigit(mode_str[0])) {
        if (parse_octal_mode(mode_str, &mode) < 0) {
            print("chmod: invalid octal mode '");
            print(mode_str);
            print("'\n");
            syscall(SYS_EXIT, 1, 0, 0);
        }
    } else {
        // Symbolic mode - for now just use a simple approach
        // We'd need to stat the file first to get current mode
        // For simplicity, start with 0 and apply the symbolic change
        uint32_t current_mode = 0644;  // Default assumption
        if (parse_symbolic_mode(mode_str, current_mode, &mode) < 0) {
            print("chmod: invalid mode '");
            print(mode_str);
            print("'\n");
            syscall(SYS_EXIT, 1, 0, 0);
        }
    }
    
    // Call chmod syscall
    long result = syscall(SYS_CHMOD, (long)path, mode, 0);
    
    if (result != 0) {
        print("chmod: cannot change permissions of '");
        print(path);
        print("'\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    
    syscall(SYS_EXIT, 0, 0, 0);
}
