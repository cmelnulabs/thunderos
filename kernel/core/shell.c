/**
 * @file shell.c
 * @brief Interactive shell for ThunderOS
 */

#include <kernel/shell.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <hal/hal_uart.h>
#include <kernel/kstring.h>
#include <fs/vfs.h>
#include <kernel/elf_loader.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

static char input_buffer[MAX_CMD_LEN];
static int input_pos = 0;

/**
 * Compare two strings
 */
static int shell_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * Concatenate two strings
 */
static char *shell_strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

/**
 * Print the shell prompt
 */
static void shell_print_prompt(void) {
    hal_uart_puts("ThunderOS> ");
}

/**
 * Print help message
 */
static void shell_help(void) {
    hal_uart_puts("ThunderOS Shell v0.4.0\n");
    hal_uart_puts("Available commands:\n");
    hal_uart_puts("  help   - Show this help message\n");
    hal_uart_puts("  clear  - Clear the screen\n");
    hal_uart_puts("  echo   - Echo arguments\n");
    hal_uart_puts("  exit   - Exit the shell\n");
    hal_uart_puts("  cat    - Display file contents\n");
    hal_uart_puts("  ls     - List directory contents\n");
}

/**
 * Clear the screen
 */
static void shell_clear(void) {
    // ANSI escape code to clear screen and move cursor to home
    hal_uart_puts("\033[2J\033[H");
}

/**
 * Echo command - print arguments
 */
static void shell_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        hal_uart_puts(argv[i]);
        if (i < argc - 1) {
            hal_uart_puts(" ");
        }
    }
    hal_uart_puts("\n");
}

/**
 * List directory contents
 */
static void shell_ls(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "/";
    
    // Resolve the directory path
    vfs_node_t *dir = vfs_resolve_path(path);
    if (!dir) {
        hal_uart_puts("ls: cannot access '");
        hal_uart_puts(path);
        hal_uart_puts("': No such file or directory\n");
        return;
    }
    
    if (dir->type != VFS_TYPE_DIRECTORY) {
        hal_uart_puts("ls: '");
        hal_uart_puts(path);
        hal_uart_puts("': Not a directory\n");
        return;
    }
    
    // List directory entries
    char name[256];
    uint32_t inode;
    uint32_t index = 0;
    
    while (dir->ops->readdir(dir, index, name, &inode) == 0) {
        hal_uart_puts(name);
        hal_uart_puts("\n");
        index++;
    }
}

/**
 * Display file contents
 */
static void shell_cat(int argc, char **argv) {
    if (argc < 2) {
        hal_uart_puts("Usage: cat <file>\n");
        return;
    }
    
    const char *path = argv[1];
    
    // Open the file
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        hal_uart_puts("cat: ");
        hal_uart_puts(path);
        hal_uart_puts(": No such file or directory\n");
        return;
    }
    
    // Read and display contents
    char buffer[256];
    int bytes_read;
    
    while ((bytes_read = vfs_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        hal_uart_puts(buffer);
    }
    
    // Close the file
    vfs_close(fd);
}

/**
 * Parse command line into arguments
 * 
 * @param cmdline Command line string
 * @param argv Array to store argument pointers
 * @param max_args Maximum number of arguments
 * @return Number of arguments parsed
 */
static int shell_parse_args(char *cmdline, char **argv, int max_args) {
    int argc = 0;
    char *p = cmdline;
    
    // Skip leading whitespace
    while (*p && (*p == ' ' || *p == '\t')) {
        p++;
    }
    
    while (*p && argc < max_args) {
        // Start of argument
        argv[argc++] = p;
        
        // Find end of argument
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        
        // Null-terminate if not end of string
        if (*p) {
            *p = '\0';
            p++;
            
            // Skip whitespace
            while (*p && (*p == ' ' || *p == '\t')) {
                p++;
            }
        }
    }
    
    return argc;
}

/**
 * Execute external program
 * 
 * @param path Path to executable
 * @param argc Number of arguments
 * @param argv Argument array
 * @return 0 on success, -1 on error
 */
static int shell_exec_program(const char *path, int argc, char **argv) {
    // Convert to const char** for elf_load_exec
    const char **const_argv = (const char **)argv;
    
    // Load ELF executable and create process
    int result = elf_load_exec(path, const_argv, argc);
    
    if (result < 0) {
        hal_uart_puts("Error: Failed to execute ");
        hal_uart_puts(path);
        hal_uart_puts("\n");
        return -1;
    }
    
    // Wait for child process to complete
    // For now, just yield a few times to let it run
    // In a real implementation, we'd use waitpid properly
    for (int i = 0; i < 100; i++) {
        extern void schedule(void);
        schedule();
    }
    
    return 0;
}

/**
 * Execute a command
 * 
 * @param cmdline Command line to execute
 */
static void shell_execute(char *cmdline) {
    char *argv[MAX_ARGS];
    int argc;
    
    // Parse command line
    argc = shell_parse_args(cmdline, argv, MAX_ARGS);
    
    if (argc == 0) {
        return;  // Empty command
    }
    
    // Check for built-in commands
    if (shell_strcmp(argv[0], "help") == 0) {
        shell_help();
    }
    else if (shell_strcmp(argv[0], "clear") == 0) {
        shell_clear();
    }
    else if (shell_strcmp(argv[0], "echo") == 0) {
        shell_echo(argc, argv);
    }
    else if (shell_strcmp(argv[0], "ls") == 0) {
        shell_ls(argc, argv);
    }
    else if (shell_strcmp(argv[0], "cat") == 0) {
        shell_cat(argc, argv);
    }
    else if (shell_strcmp(argv[0], "exit") == 0) {
        hal_uart_puts("Goodbye!\n");
        // In a real OS, we'd exit the shell process
        // For now, just return to prompt
    }
    else {
        // Try to execute as external program
        // Look in /bin directory
        char path[256];
        path[0] = '\0';
        shell_strcat(path, "/bin/");
        shell_strcat(path, argv[0]);
        
        shell_exec_program(path, argc, argv);
    }
}

/**
 * Process a character of input
 * 
 * @param c Character to process
 */
static void shell_process_char(char c) {
    if (c == '\r' || c == '\n') {
        // End of line
        hal_uart_puts("\n");
        
        // Null-terminate input
        input_buffer[input_pos] = '\0';
        
        // Execute command
        if (input_pos > 0) {
            shell_execute(input_buffer);
        }
        
        // Reset input buffer
        input_pos = 0;
        
        // Print new prompt
        shell_print_prompt();
    }
    else if (c == 127 || c == '\b') {
        // Backspace
        if (input_pos > 0) {
            input_pos--;
            hal_uart_puts("\b \b");  // Erase character
        }
    }
    else if (c >= 32 && c < 127) {
        // Printable character
        if (input_pos < MAX_CMD_LEN - 1) {
            input_buffer[input_pos++] = c;
            hal_uart_putc(c);  // Echo character
        }
    }
}

/**
 * Initialize the shell
 */
void shell_init(void) {
    hal_uart_puts("\n");
    hal_uart_puts("===========================================\n");
    hal_uart_puts("  ThunderOS v0.4.0 - Interactive Shell\n");
    hal_uart_puts("===========================================\n");
    hal_uart_puts("\n");
    hal_uart_puts("Type 'help' for available commands.\n");
    hal_uart_puts("\n");
    
    input_pos = 0;
    shell_print_prompt();
}

/**
 * Run the shell main loop
 */
void shell_run(void) {
    while (1) {
        // Read character from UART
        char c = hal_uart_getc();
        
        // Process character
        shell_process_char(c);
    }
}
