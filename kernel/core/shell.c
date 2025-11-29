/**
 * @file shell.c
 * @brief Interactive shell for ThunderOS
 */

#include <kernel/shell.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <hal/hal_uart.h>
#include <kernel/kstring.h>
#include <kernel/config.h>
#include <fs/vfs.h>
#include <kernel/elf_loader.h>
#include <drivers/vterm.h>

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
 * 
 * @param destination Destination string buffer
 * @param source Source string to append
 * @return Pointer to destination string
 */
static char *shell_strcat(char *destination, const char *source) {
    char *dest_ptr = destination;
    while (*dest_ptr) {
        dest_ptr++;
    }
    while ((*dest_ptr++ = *source++)) {
        /* Copy until null terminator */
    }
    return destination;
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
 * Clear the screen using ANSI escape codes
 */
static void shell_clear(void) {
    /* ANSI escape: clear screen and move cursor to home */
    hal_uart_puts("\033[2J\033[H");
}

/**
 * Echo command - print arguments to console
 * 
 * @param argument_count Number of arguments
 * @param argument_vector Array of argument strings
 */
static void shell_echo(int argument_count, char **argument_vector) {
    for (int arg_index = 1; arg_index < argument_count; arg_index++) {
        hal_uart_puts(argument_vector[arg_index]);
        if (arg_index < argument_count - 1) {
            hal_uart_puts(" ");
        }
    }
    hal_uart_puts("\n");
}

/**
 * List directory contents
 * 
 * @param argument_count Number of arguments
 * @param argument_vector Array of argument strings
 */
static void shell_ls(int argument_count, char **argument_vector) {
    const char *directory_path = (argument_count > 1) ? argument_vector[1] : "/";
    
    /* Resolve the directory path */
    vfs_node_t *directory_node = vfs_resolve_path(directory_path);
    if (!directory_node) {
        hal_uart_puts("ls: cannot access '");
        hal_uart_puts(directory_path);
        hal_uart_puts("': No such file or directory\n");
        return;
    }
    
    if (directory_node->type != VFS_TYPE_DIRECTORY) {
        hal_uart_puts("ls: '");
        hal_uart_puts(directory_path);
        hal_uart_puts("': Not a directory\n");
        return;
    }
    
    /* List directory entries */
    char entry_name[256];
    uint32_t entry_inode = 0;
    uint32_t entry_index = 0;
    
    while (directory_node->ops->readdir(directory_node, entry_index, entry_name, &entry_inode) == 0) {
        hal_uart_puts(entry_name);
        hal_uart_puts("\n");
        entry_index++;
    }
}

/**
 * Display file contents to console
 * 
 * @param argument_count Number of arguments
 * @param argument_vector Array of argument strings
 */
static void shell_cat(int argument_count, char **argument_vector) {
    if (argument_count < 2) {
        hal_uart_puts("Usage: cat <file>\n");
        return;
    }
    
    const char *file_path = argument_vector[1];
    
    /* Open the file */
    int file_descriptor = vfs_open(file_path, O_RDONLY);
    if (file_descriptor < 0) {
        hal_uart_puts("cat: ");
        hal_uart_puts(file_path);
        hal_uart_puts(": No such file or directory\n");
        return;
    }
    
    /* Read and display contents */
    char read_buffer[256];
    int bytes_read = 0;
    
    while ((bytes_read = vfs_read(file_descriptor, read_buffer, sizeof(read_buffer) - 1)) > 0) {
        read_buffer[bytes_read] = '\0';
        hal_uart_puts(read_buffer);
    }
    
    /* Close the file */
    vfs_close(file_descriptor);
}

/**
 * Parse command line into arguments
 * 
 * @param command_line Command line string to parse
 * @param argument_vector Array to store argument pointers
 * @param max_arguments Maximum number of arguments
 * @return Number of arguments parsed
 */
static int shell_parse_args(char *command_line, char **argument_vector, int max_arguments) {
    int argument_count = 0;
    char *parser_position = command_line;
    
    /* Skip leading whitespace */
    while (*parser_position && (*parser_position == ' ' || *parser_position == '\t')) {
        parser_position++;
    }
    
    while (*parser_position && argument_count < max_arguments) {
        /* Start of argument */
        argument_vector[argument_count++] = parser_position;
        
        /* Find end of argument */
        while (*parser_position && *parser_position != ' ' && *parser_position != '\t') {
            parser_position++;
        }
        
        /* Null-terminate if not end of string */
        if (*parser_position) {
            *parser_position = '\0';
            parser_position++;
            
            /* Skip whitespace */
            while (*parser_position && (*parser_position == ' ' || *parser_position == '\t')) {
                parser_position++;
            }
        }
    }
    
    return argument_count;
}

/**
 * Execute external program from filesystem
 * 
 * @param program_path Path to executable file
 * @param argument_count Number of arguments
 * @param argument_vector Argument array
 * @return 0 on success, -1 on error
 */
static int shell_exec_program(const char *program_path, int argument_count, char **argument_vector) {
    /* Convert to const char** for elf_load_exec */
    const char **const_argv = (const char **)argument_vector;
    
    /* Load ELF executable and create process */
    int process_id = elf_load_exec(program_path, const_argv, argument_count);
    
    if (process_id < 0) {
        hal_uart_puts("Error: Failed to execute ");
        hal_uart_puts(program_path);
        hal_uart_puts("\n");
        /* errno already set by elf_load_exec */
        return -1;
    }
    
    /* Wait for child process to complete */
    int wait_status = 0;
    extern uint64_t sys_waitpid(int pid, int *wstatus, int options);
    int result = sys_waitpid(process_id, &wait_status, 0);
    
    if (result < 0) {
        hal_uart_puts("Error: waitpid failed\n");
        /* errno already set by sys_waitpid */
        return -1;
    }
    
    return 0;
}

/**
 * Execute a shell command
 * 
 * @param command_line Command line to execute
 */
static void shell_execute(char *command_line) {
    char *argument_vector[MAX_ARGS];
    int argument_count = 0;
    
    /* Parse command line */
    argument_count = shell_parse_args(command_line, argument_vector, MAX_ARGS);
    
    if (argument_count == 0) {
        return;  /* Empty command */
    }
    
    /* Check for built-in commands */
    if (shell_strcmp(argument_vector[0], "help") == 0) {
        shell_help();
    }
    else if (shell_strcmp(argument_vector[0], "clear") == 0) {
        shell_clear();
    }
    else if (shell_strcmp(argument_vector[0], "echo") == 0) {
        shell_echo(argument_count, argument_vector);
    }
    else if (shell_strcmp(argument_vector[0], "ls") == 0) {
        shell_ls(argument_count, argument_vector);
    }
    else if (shell_strcmp(argument_vector[0], "cat") == 0) {
        shell_cat(argument_count, argument_vector);
    }
    else if (shell_strcmp(argument_vector[0], "exit") == 0) {
        hal_uart_puts("Goodbye!\n");
    }
    else {
        /* Try to execute as external program from /bin directory */
        char program_path[256];
        program_path[0] = '\0';
        shell_strcat(program_path, "/bin/");
        shell_strcat(program_path, argument_vector[0]);
        
        shell_exec_program(program_path, argument_count, argument_vector);
    }
}

/**
 * Process a character of input from console
 * 
 * @param input_char Character to process
 */
static void shell_process_char(char input_char) {
    if (input_char == '\r' || input_char == '\n') {
        /* End of line - execute command */
        hal_uart_puts("\n");
        
        /* Null-terminate input */
        input_buffer[input_pos] = '\0';
        
        /* Execute command */
        if (input_pos > 0) {
            shell_execute(input_buffer);
        }
        
        /* Reset input buffer */
        input_pos = 0;
        
        /* Print new prompt */
        shell_print_prompt();
    }
    else if (input_char == 127 || input_char == '\b') {
        /* Backspace - erase character */
        if (input_pos > 0) {
            input_pos--;
            hal_uart_puts("\b \b");
        }
    }
    else if (input_char >= ASCII_PRINTABLE_MIN && input_char < ASCII_PRINTABLE_MAX) {
        /* Printable character - echo and store */
        if (input_pos < MAX_CMD_LEN - 1) {
            input_buffer[input_pos++] = input_char;
            hal_uart_putc(input_char);
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
 * 
 * Continuously reads characters from UART and processes them
 */
void shell_run(void) {
    while (1) {
        /* Read character from UART */
        char input_char = hal_uart_getc();
        
        /* Process through virtual terminal system for Alt+Fn switching */
        if (vterm_available()) {
            input_char = vterm_process_input(input_char);
            if (input_char == 0) {
                /* Character was consumed by vterm (e.g., terminal switch) */
                continue;
            }
        }
        
        /* Process character */
        shell_process_char(input_char);
    }
}
