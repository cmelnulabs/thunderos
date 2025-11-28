/*
 * ush - ThunderOS User Shell
 * 
 * A simple user-mode shell for ThunderOS
 */

#include <stdint.h>

#define NULL ((void *)0)

// System call numbers
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_OPEN    13  // Must match kernel
#define SYS_CLOSE   14  // Must match kernel
#define SYS_FORK    7
#define SYS_WAIT    9
#define SYS_EXECVE  20

// File descriptors
#define STDIN  0
#define STDOUT 1
#define STDERR 2

// Open flags
#define O_RDONLY 0x0000

// Limits
#define MAX_CMD_LEN 256
#define MAX_ARGS 16

// Syscall wrappers
// Use __attribute__((noinline)) to ensure these are not inlined
// This prevents register corruption issues with -O0
// Syscall wrappers - implemented in syscall.S
extern long syscall0(long n);
extern long syscall1(long n, long a0);
extern long syscall2(long n, long a0, long a1);
extern long syscall3(long n, long a0, long a1, long a2);

// String functions
static int strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

// I/O functions
static void print(const char *s) {
    syscall3(SYS_WRITE, STDOUT, (long)s, strlen(s));
}

static void putchar(char c) {
    syscall3(SYS_WRITE, STDOUT, (long)&c, 1);
}

static int getchar(void) {
    char c;
    long n = syscall3(SYS_READ, STDIN, (long)&c, 1);
    return (n == 1) ? c : -1;
}

static void print_hex(unsigned long value) {
    char hex[] = "0123456789ABCDEF";
    char buf[17];
    int i = 16;
    buf[i] = '\0';
    
    if (value == 0) {
        print("0");
        return;
    }
    
    while (value > 0 && i > 0) {
        buf[--i] = hex[value & 0xF];
        value >>= 4;
    }
    
    print(&buf[i]);
}

// Shell state
static char input_buffer[MAX_CMD_LEN];
static int input_pos = 0;

// Parse command line into arguments
static int parse_args(char *cmd, char **argv, int max_args) {
    int argc = 0;
    char *p = cmd;
    
    // Skip leading whitespace
    while (*p && (*p == ' ' || *p == '\t')) p++;
    
    while (*p && argc < max_args) {
        argv[argc++] = p;
        
        // Find end of argument
        while (*p && *p != ' ' && *p != '\t') p++;
        
        if (*p) {
            *p = '\0';
            p++;
            while (*p && (*p == ' ' || *p == '\t')) p++;
        }
    }
    
    // NULL-terminate argv
    if (argc < max_args) {
        argv[argc] = NULL;
    }
    
    return argc;
}

// Built-in commands
static void cmd_help(void) {
    print("ThunderOS Shell v0.4.0 - Type 'exit' to quit\n");
}

static void cmd_clear(void) {
    print("\033[2J\033[H");
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        print(argv[i]);
        if (i < argc - 1) print(" ");
    }
    print("\n");
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        print("Usage: cat <file>\n");
        return;
    }
    
    int fd = syscall2(SYS_OPEN, (long)argv[1], O_RDONLY);
    if (fd < 0) {
        print("cat: cannot open '");
        print(argv[1]);
        print("'\n");
        return;
    }
    
    char buf[256];
    long n;
    while ((n = syscall3(SYS_READ, fd, (long)buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        print(buf);
    }
    
    syscall1(SYS_CLOSE, fd);
}

// Execute external program
// Build path from argv[0] in child after fork to avoid passing parent's stack pointers
static int exec_program(int argc, char **argv) {
    long pid = syscall0(SYS_FORK);
    
    if (pid < 0) {
        print("Error: fork failed\n");
        return -1;
    }
    
    if (pid == 0) {
        // Child process - build path HERE in child's stack
        char path[256];
        
        // If command starts with '/', use it as-is (absolute path)
        // Otherwise, prepend /bin/
        if (argv[0][0] == '/') {
            strcpy(path, argv[0]);
        } else {
            path[0] = '\0';
            strcat(path, "/bin/");
            strcat(path, argv[0]);
        }
        
        const char **exec_argv = (const char **)argv;
        const char *envp[] = { NULL };
        
        syscall3(SYS_EXECVE, (long)path, (long)exec_argv, (long)envp);
        
        // If we get here, exec failed
        print("Error: exec failed\n");
        syscall1(SYS_EXIT, 1);
    } else {
        // Parent - wait for child
        int status;
        syscall3(SYS_WAIT, pid, (long)&status, 0);
    }
    
    return 0;
}

// Execute command
static void execute_command(char *cmd) {
    char *argv[MAX_ARGS];
    int argc = parse_args(cmd, argv, MAX_ARGS);
    
    if (argc == 0) return;
    
    // Check built-in commands
    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(argv[0], "clear") == 0) {
        cmd_clear();
    } else if (strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc, argv);
    } else if (strcmp(argv[0], "exit") == 0) {
        print("Goodbye!\n");
        syscall1(SYS_EXIT, 0);
    } else {
        // Try to execute as external program
        exec_program(argc, argv);
    }
}

// Process input character
static void process_char(int c) {
    if (c == '\r' || c == '\n') {
        putchar('\n');
        input_buffer[input_pos] = '\0';
        
        if (input_pos > 0) {
            execute_command(input_buffer);
        }
        
        input_pos = 0;
        print("ThunderOS> ");
    } else if (c == 127 || c == '\b') {
        if (input_pos > 0) {
            input_pos--;
            print("\b \b");
        }
    } else if (c >= 32 && c < 127) {
        if (input_pos < MAX_CMD_LEN - 1) {
            input_buffer[input_pos++] = c;
            putchar(c);
        }
    }
}

// Main shell loop
void _start(void) {
    // CRITICAL: Initialize global pointer FIRST before any code that might use it
    // With -O0 and custom linker script, compiler doesn't auto-generate this
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    print("\n");
    print("===========================================\n");
    print("  ThunderOS v0.4.0 - User Mode Shell\n");
    print("===========================================\n");
    print("\n");
    print("Type 'help' for available commands.\n");
    print("\n");
    print("ThunderOS> ");
    
    while (1) {
        int c = getchar();
        if (c >= 0) {
            process_char(c);
        } else {
            // No input available - yield CPU
            syscall0(6); // SYS_YIELD
        }
    }
}
