/*
 * ush - ThunderOS User Shell
 * 
 * An interactive command-line shell with command history,
 * line editing, and support for built-in and external commands.
 */

/* ========================================================================
 * External syscall wrappers (defined in syscall.S)
 * ======================================================================== */

extern long syscall0(long syscall_num);
extern long syscall1(long syscall_num, long arg0);
extern long syscall2(long syscall_num, long arg0, long arg1);
extern long syscall3(long syscall_num, long arg0, long arg1, long arg2);

/* ========================================================================
 * Syscall numbers
 * ======================================================================== */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   6
#define SYS_FORK    7
#define SYS_WAIT    9
#define SYS_OPEN    13
#define SYS_CLOSE   14
#define SYS_MKDIR   17
#define SYS_RMDIR   19
#define SYS_EXECVE  20
#define SYS_CHDIR   28

/* ========================================================================
 * Constants
 * ======================================================================== */

#define HISTORY_SIZE        16
#define MAX_CMD_LEN         256
#define INPUT_BUFFER_SIZE   256
#define READ_BUFFER_SIZE    256
#define PATH_BUFFER_SIZE    64

#define STDIN_FD            0
#define STDOUT_FD           1

#define O_RDONLY            0
#define DIR_MODE            0755

#define CHAR_ESC            0x1B
#define CHAR_BACKSPACE      127
#define CHAR_BACKSPACE_ALT  '\b'

/* Escape sequence parser states */
#define ESC_STATE_NORMAL    0
#define ESC_STATE_GOT_ESC   1
#define ESC_STATE_GOT_CSI   2

/* ========================================================================
 * String constants
 * ======================================================================== */

static const char *SHELL_BANNER =
    "\n"
    "===========================================\n"
    "  ThunderOS User Shell v0.7.0\n"
    "===========================================\n"
    "\n"
    "Type 'help' for available commands\n"
    "\n";

static const char *SHELL_PROMPT = "ush> ";

static const char *HELP_TEXT =
    "Available commands:\n"
    "  help   - Show this help\n"
    "  echo   - Echo arguments\n"
    "  cat    - Display file contents\n"
    "  ls     - List directory\n"
    "  cd     - Change directory\n"
    "  pwd    - Print working directory\n"
    "  mkdir  - Create directory\n"
    "  rmdir  - Remove directory\n"
    "  clear  - Clear screen\n"
    "  hello  - Run hello program\n"
    "  clock  - Display elapsed time\n"
    "  ps     - List running processes\n"
    "  uname  - Print system information\n"
    "  uptime - Show system uptime\n"
    "  whoami - Print current user\n"
    "  tty    - Print terminal name\n"
    "  exit   - Exit shell\n";

static const char *MSG_GOODBYE = "Goodbye!\n";
static const char *MSG_UNKNOWN_CMD = "Unknown command (try 'help')\n";
static const char *MSG_USAGE_CAT = "Usage: cat <filename>\n";
static const char *MSG_USAGE_ECHO = "Usage: echo <text>\n";
static const char *MSG_USAGE_CD = "Usage: cd <directory>\n";
static const char *MSG_USAGE_MKDIR = "Usage: mkdir <directory>\n";
static const char *MSG_USAGE_RMDIR = "Usage: rmdir <directory>\n";
static const char *MSG_FILE_ERROR = "Error: Cannot open file\n";
static const char *MSG_CD_ERROR = "cd: cannot access directory\n";
static const char *MSG_MKDIR_ERROR = "mkdir: cannot create directory\n";
static const char *MSG_RMDIR_ERROR = "rmdir: cannot remove directory\n";
static const char *MSG_EXEC_FAIL = "Failed to execute program\n";

static const char *NEWLINE = "\n";
static const char *CRLF = "\r\n";
static const char *BACKSPACE_ERASE = "\b \b";
static const char *BIN_PREFIX = "/bin/";

/* ========================================================================
 * Global state
 * ======================================================================== */

/* Command history buffer */
static char g_history[HISTORY_SIZE][MAX_CMD_LEN];
static int g_history_count = 0;
static int g_history_index = 0;
static int g_browsing_history = 0;

/* Input state */
static char g_input_buffer[INPUT_BUFFER_SIZE];
static int g_input_pos = 0;
static int g_escape_state = ESC_STATE_NORMAL;

/* Path buffer for external commands */
static char g_path_buffer[PATH_BUFFER_SIZE];

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

/* String utilities */
static long str_length(const char *str);
static void print_string(const char *str);
static void print_chars(const char *chars, long count);
static int strings_equal(const char *str1, const char *str2, int length);
static void copy_string(char *dest, const char *src, int max_len);

/* History management */
static void history_init(void);
static void history_add(const char *command);
static void history_navigate_up(void);
static void history_navigate_down(void);

/* Input line management */
static void input_clear_line(void);
static void input_set_from_history(int index);

/* Command parsing */
static int parse_command_length(const char *input, int input_len);
static int parse_arg_start(const char *input, int input_len, int cmd_len);
static int command_matches(const char *input, int cmd_len, const char *command);

/* Command execution */
static void exec_external(const char *path);
static void handle_builtin_help(void);
static void handle_builtin_exit(void);
static void handle_builtin_echo(int arg_start, int input_len);
static void handle_builtin_cat(int arg_start, int input_len);
static void handle_builtin_cd(int arg_start, int input_len);
static void handle_builtin_mkdir(int arg_start, int input_len);
static void handle_builtin_rmdir(int arg_start, int input_len);
static void handle_external_command(const char *binary_name);

/* Main processing */
static void process_command(void);
static void handle_arrow_up(void);
static void handle_arrow_down(void);
static void handle_escape_sequence(char input_char);
static void handle_printable_char(char input_char);
static void handle_backspace(void);
static void handle_newline(void);

/* ========================================================================
 * String utilities
 * ======================================================================== */

/**
 * Calculate the length of a null-terminated string
 */
static long str_length(const char *str) {
    long length = 0;
    while (str[length]) {
        length++;
    }
    return length;
}

/**
 * Print a null-terminated string to stdout
 */
static void print_string(const char *str) {
    syscall3(SYS_WRITE, STDOUT_FD, (long)str, str_length(str));
}

/**
 * Print a specific number of characters to stdout
 */
static void print_chars(const char *chars, long count) {
    syscall3(SYS_WRITE, STDOUT_FD, (long)chars, count);
}

/**
 * Check if two strings are equal up to a given length
 */
static int strings_equal(const char *str1, const char *str2, int length) {
    for (int idx = 0; idx < length; idx++) {
        if (str1[idx] != str2[idx]) {
            return 0;
        }
    }
    return 1;
}

/**
 * Copy a string with maximum length limit
 */
static void copy_string(char *dest, const char *src, int max_len) {
    int idx = 0;
    while (src[idx] && idx < max_len - 1) {
        dest[idx] = src[idx];
        idx++;
    }
    dest[idx] = '\0';
}

/* ========================================================================
 * Command history management
 * ======================================================================== */

/**
 * Initialize command history
 */
static void history_init(void) {
    for (int idx = 0; idx < HISTORY_SIZE; idx++) {
        g_history[idx][0] = '\0';
    }
    g_history_count = 0;
    g_history_index = 0;
    g_browsing_history = 0;
}

/**
 * Add a command to history (if not duplicate of last command)
 */
static void history_add(const char *command) {
    /* Skip if same as last command */
    if (g_history_count > 0) {
        int cmd_length = str_length(command);
        int last_length = str_length(g_history[g_history_count - 1]);
        if (cmd_length == last_length) {
            if (strings_equal(command, g_history[g_history_count - 1], cmd_length)) {
                return;
            }
        }
    }
    
    /* Shift history if full */
    if (g_history_count >= HISTORY_SIZE) {
        for (int idx = 0; idx < HISTORY_SIZE - 1; idx++) {
            copy_string(g_history[idx], g_history[idx + 1], MAX_CMD_LEN);
        }
        g_history_count = HISTORY_SIZE - 1;
    }
    
    /* Add new command */
    copy_string(g_history[g_history_count], command, MAX_CMD_LEN);
    g_history_count++;
}

/**
 * Navigate up in command history
 */
static void history_navigate_up(void) {
    if (g_history_count == 0) {
        return;
    }
    
    if (!g_browsing_history) {
        g_browsing_history = 1;
        g_history_index = g_history_count;
    }
    
    if (g_history_index > 0) {
        g_history_index--;
        input_clear_line();
        input_set_from_history(g_history_index);
    }
}

/**
 * Navigate down in command history
 */
static void history_navigate_down(void) {
    if (!g_browsing_history) {
        return;
    }
    
    if (g_history_index < g_history_count - 1) {
        g_history_index++;
        input_clear_line();
        input_set_from_history(g_history_index);
    } else {
        /* At end of history, clear line */
        g_browsing_history = 0;
        input_clear_line();
        g_input_buffer[0] = '\0';
    }
}

/* ========================================================================
 * Input line management
 * ======================================================================== */

/**
 * Clear the current input line on screen
 */
static void input_clear_line(void) {
    while (g_input_pos > 0) {
        print_string(BACKSPACE_ERASE);
        g_input_pos--;
    }
}

/**
 * Set input buffer from history entry and display it
 */
static void input_set_from_history(int index) {
    copy_string(g_input_buffer, g_history[index], INPUT_BUFFER_SIZE);
    g_input_pos = str_length(g_input_buffer);
    print_chars(g_input_buffer, g_input_pos);
}

/* ========================================================================
 * Command parsing
 * ======================================================================== */

/**
 * Find the length of the command (up to first space)
 */
static int parse_command_length(const char *input, int input_len) {
    int cmd_len = 0;
    while (cmd_len < input_len && input[cmd_len] != ' ') {
        cmd_len++;
    }
    return cmd_len;
}

/**
 * Find the start of arguments (after command and spaces)
 */
static int parse_arg_start(const char *input, int input_len, int cmd_len) {
    int arg_start = cmd_len;
    while (arg_start < input_len && input[arg_start] == ' ') {
        arg_start++;
    }
    return arg_start;
}

/**
 * Check if input matches a command name
 */
static int command_matches(const char *input, int cmd_len, const char *command) {
    int expected_len = str_length(command);
    if (cmd_len != expected_len) {
        return 0;
    }
    return strings_equal(input, command, cmd_len);
}

/* ========================================================================
 * Command execution
 * ======================================================================== */

/**
 * Fork and execute an external program, wait for completion
 */
static void exec_external(const char *path) {
    long child_pid = syscall0(SYS_FORK);
    
    if (child_pid == 0) {
        /* Child process */
        const char *argv[] = { path, 0 };
        const char *envp[] = { 0 };
        syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
        
        /* Exec failed */
        print_string(MSG_EXEC_FAIL);
        syscall1(SYS_EXIT, 1);
    } else if (child_pid > 0) {
        /* Parent - wait for child */
        int exit_status = 0;
        syscall3(SYS_WAIT, child_pid, (long)&exit_status, 0);
    }
}

/**
 * Handle 'help' command
 */
static void handle_builtin_help(void) {
    print_string(HELP_TEXT);
}

/**
 * Handle 'exit' command
 */
static void handle_builtin_exit(void) {
    print_string(MSG_GOODBYE);
    syscall1(SYS_EXIT, 0);
}

/**
 * Handle 'echo' command
 */
static void handle_builtin_echo(int arg_start, int input_len) {
    if (arg_start < input_len) {
        print_chars(&g_input_buffer[arg_start], input_len - arg_start);
        print_string(NEWLINE);
    } else {
        print_string(MSG_USAGE_ECHO);
    }
}

/**
 * Handle 'cat' command - display file contents
 */
static void handle_builtin_cat(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_CAT);
        return;
    }
    
    /* Null-terminate the filename */
    g_input_buffer[input_len] = '\0';
    
    long file_fd = syscall3(SYS_OPEN, (long)&g_input_buffer[arg_start], O_RDONLY, 0);
    if (file_fd < 0) {
        print_string(MSG_FILE_ERROR);
        return;
    }
    
    /* Read and display file contents */
    char read_buffer[READ_BUFFER_SIZE];
    long bytes_read;
    while ((bytes_read = syscall3(SYS_READ, file_fd, (long)read_buffer, READ_BUFFER_SIZE - 1)) > 0) {
        print_chars(read_buffer, bytes_read);
    }
    
    syscall1(SYS_CLOSE, file_fd);
}

/**
 * Handle 'cd' command - change directory
 */
static void handle_builtin_cd(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_CD);
        return;
    }
    
    g_input_buffer[input_len] = '\0';
    long result = syscall1(SYS_CHDIR, (long)&g_input_buffer[arg_start]);
    if (result != 0) {
        print_string(MSG_CD_ERROR);
    }
}

/**
 * Handle 'mkdir' command - create directory
 */
static void handle_builtin_mkdir(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_MKDIR);
        return;
    }
    
    g_input_buffer[input_len] = '\0';
    long result = syscall2(SYS_MKDIR, (long)&g_input_buffer[arg_start], DIR_MODE);
    if (result != 0) {
        print_string(MSG_MKDIR_ERROR);
    }
}

/**
 * Handle 'rmdir' command - remove directory
 */
static void handle_builtin_rmdir(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_RMDIR);
        return;
    }
    
    g_input_buffer[input_len] = '\0';
    long result = syscall1(SYS_RMDIR, (long)&g_input_buffer[arg_start]);
    if (result != 0) {
        print_string(MSG_RMDIR_ERROR);
    }
}

/**
 * Handle external command - fork and exec from /bin
 */
static void handle_external_command(const char *binary_name) {
    /* Build path: /bin/<binary_name> */
    int path_idx = 0;
    int prefix_idx = 0;
    
    while (BIN_PREFIX[prefix_idx]) {
        g_path_buffer[path_idx++] = BIN_PREFIX[prefix_idx++];
    }
    
    int name_idx = 0;
    while (binary_name[name_idx] && path_idx < PATH_BUFFER_SIZE - 1) {
        g_path_buffer[path_idx++] = binary_name[name_idx++];
    }
    g_path_buffer[path_idx] = '\0';
    
    exec_external(g_path_buffer);
}

/**
 * Process the entered command
 */
static void process_command(void) {
    int input_len = g_input_pos;
    g_input_buffer[input_len] = '\0';
    
    if (input_len == 0) {
        return;
    }
    
    /* Add to history */
    history_add(g_input_buffer);
    
    /* Parse command */
    int cmd_len = parse_command_length(g_input_buffer, input_len);
    int arg_start = parse_arg_start(g_input_buffer, input_len, cmd_len);
    
    /* Check built-in commands first */
    if (command_matches(g_input_buffer, cmd_len, "help")) {
        handle_builtin_help();
    } else if (command_matches(g_input_buffer, cmd_len, "exit")) {
        handle_builtin_exit();
    } else if (command_matches(g_input_buffer, cmd_len, "echo")) {
        handle_builtin_echo(arg_start, input_len);
    } else if (command_matches(g_input_buffer, cmd_len, "cat")) {
        handle_builtin_cat(arg_start, input_len);
    } else if (command_matches(g_input_buffer, cmd_len, "cd")) {
        handle_builtin_cd(arg_start, input_len);
    } else if (command_matches(g_input_buffer, cmd_len, "mkdir")) {
        handle_builtin_mkdir(arg_start, input_len);
    } else if (command_matches(g_input_buffer, cmd_len, "rmdir")) {
        handle_builtin_rmdir(arg_start, input_len);
    }
    /* External commands - fork and exec from /bin */
    else if (command_matches(g_input_buffer, cmd_len, "ls")) {
        handle_external_command("ls");
    } else if (command_matches(g_input_buffer, cmd_len, "pwd")) {
        handle_external_command("pwd");
    } else if (command_matches(g_input_buffer, cmd_len, "clear")) {
        handle_external_command("clear");
    } else if (command_matches(g_input_buffer, cmd_len, "hello")) {
        handle_external_command("hello");
    } else if (command_matches(g_input_buffer, cmd_len, "clock")) {
        handle_external_command("clock");
    } else if (command_matches(g_input_buffer, cmd_len, "ps")) {
        handle_external_command("ps");
    } else if (command_matches(g_input_buffer, cmd_len, "uname")) {
        handle_external_command("uname");
    } else if (command_matches(g_input_buffer, cmd_len, "uptime")) {
        handle_external_command("uptime");
    } else if (command_matches(g_input_buffer, cmd_len, "whoami")) {
        handle_external_command("whoami");
    } else if (command_matches(g_input_buffer, cmd_len, "tty")) {
        handle_external_command("tty");
    } else {
        print_string(MSG_UNKNOWN_CMD);
    }
}

/* ========================================================================
 * Input handling
 * ======================================================================== */

/**
 * Handle up arrow key - navigate history backward
 */
static void handle_arrow_up(void) {
    history_navigate_up();
}

/**
 * Handle down arrow key - navigate history forward
 */
static void handle_arrow_down(void) {
    history_navigate_down();
}

/**
 * Handle escape sequence characters
 */
static void handle_escape_sequence(char input_char) {
    if (g_escape_state == ESC_STATE_GOT_ESC) {
        if (input_char == '[') {
            g_escape_state = ESC_STATE_GOT_CSI;
        } else {
            g_escape_state = ESC_STATE_NORMAL;
        }
    } else if (g_escape_state == ESC_STATE_GOT_CSI) {
        g_escape_state = ESC_STATE_NORMAL;
        if (input_char == 'A') {
            handle_arrow_up();
        } else if (input_char == 'B') {
            handle_arrow_down();
        }
        /* Ignore other escape sequences */
    }
}

/**
 * Handle printable character input
 */
static void handle_printable_char(char input_char) {
    print_chars(&input_char, 1);
    if (g_input_pos < INPUT_BUFFER_SIZE - 1) {
        g_input_buffer[g_input_pos++] = input_char;
    }
}

/**
 * Handle backspace key
 */
static void handle_backspace(void) {
    if (g_input_pos > 0) {
        g_input_pos--;
        print_string(BACKSPACE_ERASE);
    }
}

/**
 * Handle newline (Enter key)
 */
static void handle_newline(void) {
    print_string(CRLF);
    g_browsing_history = 0;
    process_command();
    g_input_pos = 0;
    print_string(SHELL_PROMPT);
}

/* ========================================================================
 * Main entry point
 * ======================================================================== */

void _start(void) {
    /* Initialize global pointer for global variable access */
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    /* Initialize shell state */
    history_init();
    g_input_pos = 0;
    g_escape_state = ESC_STATE_NORMAL;
    
    /* Display banner */
    print_string(SHELL_BANNER);
    print_string(SHELL_PROMPT);
    
    /* Main input loop */
    while (1) {
        char input_char;
        long bytes_read = syscall3(SYS_READ, STDIN_FD, (long)&input_char, 1);
        
        if (bytes_read != 1) {
            syscall0(SYS_YIELD);
            continue;
        }
        
        /* Handle escape sequences */
        if (g_escape_state != ESC_STATE_NORMAL) {
            handle_escape_sequence(input_char);
            continue;
        }
        
        /* Check for escape character */
        if (input_char == CHAR_ESC) {
            g_escape_state = ESC_STATE_GOT_ESC;
            continue;
        }
        
        /* Handle special characters */
        if (input_char == '\r' || input_char == '\n') {
            handle_newline();
        } else if (input_char == CHAR_BACKSPACE || input_char == CHAR_BACKSPACE_ALT) {
            handle_backspace();
        } else if (input_char >= 32 && input_char < 127) {
            handle_printable_char(input_char);
        }
    }
}
