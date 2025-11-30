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
#define SYS_PIPE    26
#define SYS_GETDENTS 27
#define SYS_CHDIR   28
#define SYS_GETCWD  29
#define SYS_DUP2    35
#define SYS_SETFGPID 36

/* ========================================================================
 * Constants
 * ======================================================================== */

#define HISTORY_SIZE        16
#define MAX_CMD_LEN         256
#define INPUT_BUFFER_SIZE   256
#define READ_BUFFER_SIZE    256
#define PATH_BUFFER_SIZE    64
#define MAX_ENV_VARS        32
#define MAX_ENV_NAME        32
#define MAX_ENV_VALUE       128
#define EXPANDED_CMD_SIZE   512
#define MAX_ARGS            16

#define STDIN_FD            0
#define STDOUT_FD           1

#define O_RDONLY            0x0000
#define O_WRONLY            0x0001
#define O_CREAT             0x0040
#define O_TRUNC             0x0200
#define O_APPEND            0x0400
#define DIR_MODE            0755
#define FILE_MODE           0644

#define CHAR_ESC            0x1B
#define CHAR_TAB            0x09
#define CHAR_BACKSPACE      127
#define CHAR_BACKSPACE_ALT  '\b'

/* Escape sequence parser states */
#define ESC_STATE_NORMAL    0
#define ESC_STATE_GOT_ESC   1
#define ESC_STATE_GOT_CSI   2

/* Directory entry structure (matches kernel) */
struct thunderos_dirent {
    unsigned int   d_ino;       /* Inode number */
    unsigned short d_reclen;    /* Record length */
    unsigned char  d_type;      /* File type */
    char           d_name[256]; /* File name (null-terminated) */
};

/* File types */
#define DT_REG   1  /* Regular file */
#define DT_DIR   2  /* Directory */

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
    "Built-in commands:\n"
    "  help     - Show this help\n"
    "  echo     - Echo arguments (supports $VAR)\n"
    "  cat      - Display file contents\n"
    "  cd       - Change directory\n"
    "  source   - Run commands from script file\n"
    "  export   - Set environment variable (VAR=value)\n"
    "  unset    - Remove environment variable\n"
    "  env      - List environment variables\n"
    "  history  - Show command history\n"
    "  exit     - Exit shell\n"
    "\n"
    "File utilities:\n"
    "  ls       - List directory\n"
    "  pwd      - Print working directory\n"
    "  mkdir    - Create directory\n"
    "  rmdir    - Remove directory\n"
    "  touch    - Create empty file\n"
    "  rm       - Remove file\n"
    "  clear    - Clear screen\n"
    "\n"
    "System utilities:\n"
    "  ps       - List processes\n"
    "  kill     - Send signal to process\n"
    "  sleep    - Sleep for seconds\n"
    "  uname    - System information\n"
    "  uptime   - System uptime\n"
    "  whoami   - Current user\n"
    "  tty      - Terminal name\n"
    "\n"
    "Other programs: hello, clock\n"
    "\n"
    "Features:\n"
    "  - Tab: Filename completion\n"
    "  - Up/Down arrows: Navigate command history\n"
    "  - $VAR or ${VAR}: Variable expansion\n"
    "  - cmd1 | cmd2: Pipe output to input\n"
    "  - < file: Redirect input\n"
    "  - > file: Redirect output (overwrite)\n"
    "  - >> file: Redirect output (append)\n"
    "  - Relative paths: cd .., cat file\n"
    "\n";

static const char *MSG_GOODBYE = "Goodbye!\n";
static const char *MSG_UNKNOWN_CMD = "Unknown command (try 'help')\n";
static const char *MSG_USAGE_CAT = "Usage: cat <filename>\n";
static const char *MSG_USAGE_ECHO = "Usage: echo <text>\n";
static const char *MSG_USAGE_CD = "Usage: cd <directory>\n";
static const char *MSG_USAGE_MKDIR = "Usage: mkdir <directory>\n";
static const char *MSG_USAGE_RMDIR = "Usage: rmdir <directory>\n";
static const char *MSG_USAGE_SOURCE = "Usage: source <script>\n";
static const char *MSG_FILE_ERROR = "Error: Cannot open file\n";
static const char *MSG_CD_ERROR = "cd: cannot access directory\n";
static const char *MSG_MKDIR_ERROR = "mkdir: cannot create directory\n";
static const char *MSG_RMDIR_ERROR = "rmdir: cannot remove directory\n";
static const char *MSG_EXEC_FAIL = "Failed to execute program\n";
static const char *MSG_USAGE_EXPORT = "Usage: export VAR=value\n";
static const char *MSG_ENV_FULL = "export: too many variables\n";
static const char *MSG_REDIR_ERROR = "Error: Cannot open output file\n";

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

/* Environment variables */
typedef struct {
    char name[MAX_ENV_NAME];
    char value[MAX_ENV_VALUE];
    int in_use;
} env_var_t;

static env_var_t g_env_vars[MAX_ENV_VARS];

/* Expanded command buffer (after variable substitution) */
static char g_expanded_buffer[EXPANDED_CMD_SIZE];

/* I/O redirection state */
static char *g_redir_output_file;
static int g_redir_append;

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

/* String utilities */
static long str_length(const char *str);
static void print_string(const char *str);
static void print_chars(const char *chars, long count);
static void print_char(char c);
static int strings_equal(const char *str1, const char *str2, int length);
static void copy_string(char *dest, const char *src, int max_len);

/* History management */
static void history_init(void);
static void history_add(const char *command);
static void history_navigate_up(void);
static void history_navigate_down(void);

/* Environment variables */
static void env_init(void);
static const char *env_get(const char *name);
static int env_set(const char *name, const char *value);
static int env_unset(const char *name);
static void env_expand(const char *input, char *output, int max_len);

/* I/O redirection */
static void parse_redirections(char *cmd, char **input_file, char **output_file, int *append);

/* Input line management */
static void input_clear_line(void);
static void input_set_from_history(int index);

/* Command parsing */
static int parse_command_length(const char *input, int input_len);
static int parse_arg_start(const char *input, int input_len, int cmd_len);
static int command_matches(const char *input, int cmd_len, const char *command);

/* Command execution */
static void exec_external_with_args(const char *path, int arg_start, int input_len);
static void handle_builtin_help(void);
static void handle_builtin_exit(void);
static void handle_builtin_echo(int arg_start, int input_len);
static void handle_builtin_cat(int arg_start, int input_len, long input_fd);
static void handle_builtin_cd(int arg_start, int input_len);
static void handle_builtin_mkdir(int arg_start, int input_len);
static void handle_builtin_rmdir(int arg_start, int input_len);
static void handle_builtin_source(int arg_start, int input_len);
static void handle_external_command(const char *binary_name, int arg_start, int input_len);

/* Main processing */
static void process_command(void);
static int find_pipe_char(const char *cmd);
static void execute_pipeline(char *left_cmd, char *right_cmd);
static void handle_arrow_up(void);
static void handle_arrow_down(void);
static void handle_escape_sequence(char input_char);
static void handle_printable_char(char input_char);
static void handle_backspace(void);
static void handle_newline(void);
static void handle_tab(void);
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
 * Print a single character to stdout
 */
static void print_char(char c) {
    syscall3(SYS_WRITE, STDOUT_FD, (long)&c, 1);
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
 * Environment variable management
 * ======================================================================== */

/**
 * Initialize environment variables
 */
static void env_init(void) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        g_env_vars[i].in_use = 0;
    }
    /* Set default environment variables */
    env_set("PATH", "/bin");
    env_set("HOME", "/");
    env_set("SHELL", "/bin/ush");
    env_set("USER", "root");
}

/**
 * Get environment variable value
 */
static const char *env_get(const char *name) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].in_use) {
            if (strings_equal(g_env_vars[i].name, name, str_length(name)) &&
                g_env_vars[i].name[str_length(name)] == '\0') {
                return g_env_vars[i].value;
            }
        }
    }
    return (const char *)0;
}

/**
 * Set environment variable
 */
static int env_set(const char *name, const char *value) {
    /* Check if already exists */
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].in_use) {
            int name_len = str_length(name);
            if (strings_equal(g_env_vars[i].name, name, name_len) &&
                g_env_vars[i].name[name_len] == '\0') {
                copy_string(g_env_vars[i].value, value, MAX_ENV_VALUE);
                return 0;
            }
        }
    }
    
    /* Find empty slot */
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (!g_env_vars[i].in_use) {
            copy_string(g_env_vars[i].name, name, MAX_ENV_NAME);
            copy_string(g_env_vars[i].value, value, MAX_ENV_VALUE);
            g_env_vars[i].in_use = 1;
            return 0;
        }
    }
    
    return -1;  /* No space */
}

/**
 * Unset environment variable
 */
static int env_unset(const char *name) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].in_use) {
            int name_len = str_length(name);
            if (strings_equal(g_env_vars[i].name, name, name_len) &&
                g_env_vars[i].name[name_len] == '\0') {
                g_env_vars[i].in_use = 0;
                return 0;
            }
        }
    }
    return -1;  /* Not found */
}

/**
 * Expand environment variables in a string
 * Supports $VAR and ${VAR} syntax
 */
static void env_expand(const char *input, char *output, int max_len) {
    int in_pos = 0;
    int out_pos = 0;
    
    while (input[in_pos] && out_pos < max_len - 1) {
        if (input[in_pos] == '$') {
            in_pos++;
            char var_name[MAX_ENV_NAME];
            int var_pos = 0;
            int braces = 0;
            
            /* Handle ${VAR} syntax */
            if (input[in_pos] == '{') {
                braces = 1;
                in_pos++;
            }
            
            /* Extract variable name */
            while (input[in_pos] && var_pos < MAX_ENV_NAME - 1) {
                char c = input[in_pos];
                if (braces) {
                    if (c == '}') {
                        in_pos++;
                        break;
                    }
                } else {
                    /* Variable names: alphanumeric and underscore */
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '_')) {
                        break;
                    }
                }
                var_name[var_pos++] = c;
                in_pos++;
            }
            var_name[var_pos] = '\0';
            
            /* Look up and substitute */
            const char *value = env_get(var_name);
            if (value) {
                while (*value && out_pos < max_len - 1) {
                    output[out_pos++] = *value++;
                }
            }
        } else {
            output[out_pos++] = input[in_pos++];
        }
    }
    output[out_pos] = '\0';
}

/* ========================================================================
 * I/O Redirection parsing
 * ======================================================================== */

/**
 * Parse command for I/O redirections (<, >, >>)
 * Modifies cmd in place to remove redirection operators
 */
static void parse_redirections(char *cmd, char **input_file, char **output_file, int *append) {
    *input_file = (char *)0;
    *output_file = (char *)0;
    *append = 0;
    
    char *p = cmd;
    while (*p) {
        if (*p == '<') {
            /* Input redirection */
            *p = '\0';
            p++;
            
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;
            
            /* Get filename */
            *input_file = p;
            
            /* Find end of filename */
            while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '<') p++;
            if (*p) {
                char next = *p;
                *p = '\0';
                if (next != ' ' && next != '\t') {
                    /* There's another redirection, continue parsing */
                    p++;
                    continue;
                }
                p++;
            }
        } else if (*p == '>') {
            if (*(p + 1) == '>') {
                /* Append mode >> */
                *append = 1;
                *p = '\0';
                p += 2;
            } else {
                /* Truncate mode > */
                *p = '\0';
                p++;
            }
            
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;
            
            /* Get filename */
            *output_file = p;
            
            /* Find end of filename */
            while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '<') p++;
            if (*p) {
                char next = *p;
                *p = '\0';
                if (next != ' ' && next != '\t') {
                    p++;
                    continue;
                }
                p++;
            }
        } else {
            p++;
        }
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

/* Static buffer for argument strings */
static char g_arg_buffer[EXPANDED_CMD_SIZE];

/**
 * Fork and execute an external program with arguments, wait for completion
 */
static void exec_external_with_args(const char *path, int arg_start, int input_len) {
    /* Build argv array by parsing arguments from g_expanded_buffer */
    const char *argv[MAX_ARGS];
    int argc = 0;
    
    /* argv[0] is the program path */
    argv[argc++] = path;
    
    /* Copy arguments to our buffer and parse them */
    if (arg_start < input_len) {
        /* Copy the argument portion */
        int i = 0;
        for (int j = arg_start; j < input_len && i < EXPANDED_CMD_SIZE - 1; j++) {
            g_arg_buffer[i++] = g_expanded_buffer[j];
        }
        g_arg_buffer[i] = '\0';
        
        /* Parse arguments (space-separated) */
        char *p = g_arg_buffer;
        while (*p && argc < MAX_ARGS - 1) {
            /* Skip leading whitespace */
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            
            /* Start of argument */
            argv[argc++] = p;
            
            /* Find end of argument */
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) {
                *p++ = '\0';  /* Null-terminate this argument */
            }
        }
    }
    
    /* Null-terminate argv */
    argv[argc] = (char *)0;
    
    long child_pid = syscall0(SYS_FORK);
    
    if (child_pid == 0) {
        /* Child process */
        const char *envp[] = { 0 };
        syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
        
        /* Exec failed */
        print_string(MSG_EXEC_FAIL);
        syscall1(SYS_EXIT, 1);
    } else if (child_pid > 0) {
        /* Parent - set child as foreground process for Ctrl+C */
        syscall1(SYS_SETFGPID, child_pid);
        
        /* Wait for child */
        int exit_status = 0;
        syscall3(SYS_WAIT, child_pid, (long)&exit_status, 0);
        
        /* Clear foreground process (back to shell) */
        syscall1(SYS_SETFGPID, -1);
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
static void handle_builtin_cat(int arg_start, int input_len, long input_fd) {
    long file_fd;
    
    /* If no filename but input redirection, use input_fd */
    if (arg_start >= input_len) {
        if (input_fd >= 0) {
            file_fd = input_fd;
        } else {
            print_string(MSG_USAGE_CAT);
            return;
        }
    } else {
        /* Null-terminate the filename */
        g_expanded_buffer[input_len] = '\0';
        
        file_fd = syscall3(SYS_OPEN, (long)&g_expanded_buffer[arg_start], O_RDONLY, 0);
        if (file_fd < 0) {
            print_string(MSG_FILE_ERROR);
            return;
        }
    }
    
    /* Read and display file contents */
    char read_buffer[READ_BUFFER_SIZE];
    long bytes_read;
    while ((bytes_read = syscall3(SYS_READ, file_fd, (long)read_buffer, READ_BUFFER_SIZE - 1)) > 0) {
        print_chars(read_buffer, bytes_read);
    }
    
    /* Only close if we opened it (not from input redirection) */
    if (arg_start < input_len) {
        syscall1(SYS_CLOSE, file_fd);
    }
}

/**
 * Handle 'cd' command - change directory
 */
static void handle_builtin_cd(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_CD);
        return;
    }
    
    g_expanded_buffer[input_len] = '\0';
    long result = syscall1(SYS_CHDIR, (long)&g_expanded_buffer[arg_start]);
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
    
    g_expanded_buffer[input_len] = '\0';
    long result = syscall2(SYS_MKDIR, (long)&g_expanded_buffer[arg_start], DIR_MODE);
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
    
    g_expanded_buffer[input_len] = '\0';
    long result = syscall1(SYS_RMDIR, (long)&g_expanded_buffer[arg_start]);
    if (result != 0) {
        print_string(MSG_RMDIR_ERROR);
    }
}

/**
 * Handle 'export' command - set environment variable
 */
static void handle_builtin_export(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_EXPORT);
        return;
    }
    
    /* Find '=' in the argument */
    char *arg = &g_expanded_buffer[arg_start];
    char *eq = arg;
    while (*eq && *eq != '=') eq++;
    
    if (*eq != '=') {
        print_string(MSG_USAGE_EXPORT);
        return;
    }
    
    /* Split into name and value */
    *eq = '\0';
    const char *name = arg;
    const char *value = eq + 1;
    
    if (env_set(name, value) < 0) {
        print_string(MSG_ENV_FULL);
    }
}

/**
 * Handle 'unset' command - remove environment variable
 */
static void handle_builtin_unset(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string("Usage: unset VAR\n");
        return;
    }
    g_expanded_buffer[input_len] = '\0';
    env_unset(&g_expanded_buffer[arg_start]);
}

/**
 * Handle 'env' command - list environment variables
 */
static void handle_builtin_env(void) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].in_use) {
            print_string(g_env_vars[i].name);
            print_string("=");
            print_string(g_env_vars[i].value);
            print_string(NEWLINE);
        }
    }
}

/**
 * Handle 'history' command - show command history
 */
static void handle_builtin_history(void) {
    char num_buf[8];
    for (int i = 0; i < g_history_count; i++) {
        /* Print index */
        int idx = i + 1;
        int pos = 0;
        if (idx >= 10) {
            num_buf[pos++] = '0' + (idx / 10);
        }
        num_buf[pos++] = '0' + (idx % 10);
        num_buf[pos++] = ' ';
        num_buf[pos++] = ' ';
        num_buf[pos] = '\0';
        print_string(num_buf);
        print_string(g_history[i]);
        print_string(NEWLINE);
    }
}

/**
 * Handle external command - fork and exec from /bin with arguments
 */
static void handle_external_command(const char *binary_name, int arg_start, int input_len) {
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
    
    exec_external_with_args(g_path_buffer, arg_start, input_len);
}

/**
 * Write string to file descriptor
 */
static void write_to_fd(long fd, const char *str) {
    syscall3(SYS_WRITE, fd, (long)str, str_length(str));
}

/* Forward declaration for execute_script_line */
static void execute_script_line(char *line);

/**
 * Handle 'source' command - run commands from a script file
 */
static void handle_builtin_source(int arg_start, int input_len) {
    if (arg_start >= input_len) {
        print_string(MSG_USAGE_SOURCE);
        return;
    }
    
    /* Null-terminate the filename */
    g_expanded_buffer[input_len] = '\0';
    
    long file_fd = syscall3(SYS_OPEN, (long)&g_expanded_buffer[arg_start], O_RDONLY, 0);
    if (file_fd < 0) {
        print_string(MSG_FILE_ERROR);
        return;
    }
    
    /* Read and execute script line by line */
    char line_buffer[MAX_CMD_LEN];
    int line_pos = 0;
    char ch;
    
    while (syscall3(SYS_READ, file_fd, (long)&ch, 1) == 1) {
        if (ch == '\n' || ch == '\r') {
            if (line_pos > 0) {
                line_buffer[line_pos] = '\0';
                
                /* Skip comment lines */
                int first_char = 0;
                while (first_char < line_pos && 
                       (line_buffer[first_char] == ' ' || line_buffer[first_char] == '\t')) {
                    first_char++;
                }
                
                if (first_char < line_pos && line_buffer[first_char] != '#') {
                    /* Echo the command being executed */
                    print_string("+ ");
                    print_string(line_buffer);
                    print_string(NEWLINE);
                    
                    /* Execute the line */
                    execute_script_line(line_buffer);
                }
                line_pos = 0;
            }
        } else if (line_pos < MAX_CMD_LEN - 1) {
            line_buffer[line_pos++] = ch;
        }
    }
    
    /* Execute any remaining content */
    if (line_pos > 0) {
        line_buffer[line_pos] = '\0';
        int first_char = 0;
        while (first_char < line_pos && 
               (line_buffer[first_char] == ' ' || line_buffer[first_char] == '\t')) {
            first_char++;
        }
        if (first_char < line_pos && line_buffer[first_char] != '#') {
            print_string("+ ");
            print_string(line_buffer);
            print_string(NEWLINE);
            execute_script_line(line_buffer);
        }
    }
    
    syscall1(SYS_CLOSE, file_fd);
}

/**
 * Execute a single line from a script (or directly)
 * This copies the line to g_input_buffer and processes it
 */
static void execute_script_line(char *line) {
    /* Save current input buffer state */
    int saved_pos = g_input_pos;
    
    /* Copy line to input buffer */
    int i = 0;
    while (line[i] && i < INPUT_BUFFER_SIZE - 1) {
        g_input_buffer[i] = line[i];
        i++;
    }
    g_input_buffer[i] = '\0';
    g_input_pos = i;
    
    /* Process the command (without adding to history - it's a script) */
    int input_len = g_input_pos;
    
    /* Expand environment variables */
    env_expand(g_input_buffer, g_expanded_buffer, EXPANDED_CMD_SIZE);
    
    /* Parse for I/O redirections */
    char *input_file = (char *)0;
    char *output_file = (char *)0;
    int append_mode = 0;
    parse_redirections(g_expanded_buffer, &input_file, &output_file, &append_mode);
    
    /* Open input file if redirected */
    long input_fd = -1;
    if (input_file && input_file[0]) {
        input_fd = syscall3(SYS_OPEN, (long)input_file, O_RDONLY, 0);
        if (input_fd < 0) {
            print_string("Error: Cannot open input file\n");
            g_input_pos = saved_pos;
            return;
        }
    }
    
    /* Open output file if redirected */
    long output_fd = -1;
    if (output_file && output_file[0]) {
        int flags = O_WRONLY | O_CREAT;
        if (append_mode) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        output_fd = syscall3(SYS_OPEN, (long)output_file, flags, FILE_MODE);
        if (output_fd < 0) {
            print_string(MSG_REDIR_ERROR);
            if (input_fd >= 0) syscall1(SYS_CLOSE, input_fd);
            g_input_pos = saved_pos;
            return;
        }
    }
    
    /* Recalculate length after redirection parsing */
    int expanded_len = str_length(g_expanded_buffer);
    
    /* Trim trailing whitespace */
    while (expanded_len > 0 && (g_expanded_buffer[expanded_len - 1] == ' ' || 
                                 g_expanded_buffer[expanded_len - 1] == '\t')) {
        expanded_len--;
    }
    g_expanded_buffer[expanded_len] = '\0';
    
    /* Parse command */
    int cmd_len = parse_command_length(g_expanded_buffer, expanded_len);
    int arg_start = parse_arg_start(g_expanded_buffer, expanded_len, cmd_len);
    
    /* Execute built-in or external command (simplified version) */
    if (command_matches(g_expanded_buffer, cmd_len, "echo")) {
        if (output_fd >= 0 && arg_start < expanded_len) {
            write_to_fd(output_fd, &g_expanded_buffer[arg_start]);
            write_to_fd(output_fd, NEWLINE);
        } else if (arg_start < expanded_len) {
            print_chars(&g_expanded_buffer[arg_start], expanded_len - arg_start);
            print_string(NEWLINE);
        }
    } else if (command_matches(g_expanded_buffer, cmd_len, "cat")) {
        handle_builtin_cat(arg_start, expanded_len, input_fd);
    } else if (command_matches(g_expanded_buffer, cmd_len, "cd")) {
        handle_builtin_cd(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "mkdir")) {
        handle_builtin_mkdir(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "rmdir")) {
        handle_builtin_rmdir(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "export")) {
        handle_builtin_export(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "unset")) {
        handle_builtin_unset(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "ls")) {
        handle_external_command("ls", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "pwd")) {
        handle_external_command("pwd", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "clear")) {
        handle_external_command("clear", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "hello")) {
        handle_external_command("hello", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "sleep")) {
        handle_external_command("sleep", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "touch")) {
        handle_external_command("touch", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "rm")) {
        handle_external_command("rm", arg_start, expanded_len);
    } else if (expanded_len > 0) {
        print_string(MSG_UNKNOWN_CMD);
    }
    
    /* Close opened file descriptors */
    if (output_fd >= 0) {
        syscall1(SYS_CLOSE, output_fd);
    }
    if (input_fd >= 0) {
        syscall1(SYS_CLOSE, input_fd);
    }
    
    /* Restore input buffer state */
    g_input_pos = saved_pos;
}

/**
 * Find pipe character in command string
 * Returns index of '|' or -1 if not found
 */
static int find_pipe_char(const char *cmd) {
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] == '|') {
            return i;
        }
    }
    return -1;
}

/**
 * Execute a simple command (no pipes) - used by pipeline
 * Forks, execs, and waits
 */
static void exec_simple_command(const char *cmd) {
    /* Skip leading whitespace */
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    
    if (*cmd == '\0') return;
    
    /* Parse command name */
    char cmd_name[64];
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && i < 63) {
        cmd_name[i] = cmd[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    /* Build path */
    char path[PATH_BUFFER_SIZE];
    path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
    int j = 5;
    for (int k = 0; cmd_name[k] && j < PATH_BUFFER_SIZE - 1; k++) {
        path[j++] = cmd_name[k];
    }
    path[j] = '\0';
    
    /* Skip to arguments */
    while (cmd[i] == ' ' || cmd[i] == '\t') i++;
    
    /* Build argv */
    const char *argv[MAX_ARGS];
    int argc = 0;
    argv[argc++] = path;
    
    /* Parse remaining arguments */
    static char arg_buf[256];
    int arg_idx = 0;
    const char *arg_start_ptr = &cmd[i];
    
    while (*arg_start_ptr && argc < MAX_ARGS - 1) {
        while (*arg_start_ptr == ' ' || *arg_start_ptr == '\t') arg_start_ptr++;
        if (!*arg_start_ptr) break;
        
        argv[argc++] = &arg_buf[arg_idx];
        while (*arg_start_ptr && *arg_start_ptr != ' ' && *arg_start_ptr != '\t') {
            if (arg_idx < 255) {
                arg_buf[arg_idx++] = *arg_start_ptr;
            }
            arg_start_ptr++;
        }
        arg_buf[arg_idx++] = '\0';
    }
    argv[argc] = (char *)0;
    
    /* Execute */
    const char *envp[] = { 0 };
    syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
    
    /* If we get here, exec failed */
    print_string("exec failed: ");
    print_string(cmd_name);
    print_string("\n");
    syscall1(SYS_EXIT, 1);
}

/**
 * Execute a pipeline: left_cmd | right_cmd
 */
static void execute_pipeline(char *left_cmd, char *right_cmd) {
    int pipefd[2];
    
    /* Create pipe */
    if (syscall1(SYS_PIPE, (long)pipefd) < 0) {
        print_string("Error: Cannot create pipe\n");
        return;
    }
    
    /* Fork for left command (writes to pipe) */
    long left_pid = syscall0(SYS_FORK);
    
    if (left_pid == 0) {
        /* Left child: stdout -> pipe write end */
        syscall1(SYS_CLOSE, pipefd[0]);  /* Close read end */
        syscall2(SYS_DUP2, pipefd[1], STDOUT_FD);
        syscall1(SYS_CLOSE, pipefd[1]);
        
        exec_simple_command(left_cmd);
        /* Never returns */
    }
    
    /* Fork for right command (reads from pipe) */
    long right_pid = syscall0(SYS_FORK);
    
    if (right_pid == 0) {
        /* Right child: stdin <- pipe read end */
        syscall1(SYS_CLOSE, pipefd[1]);  /* Close write end */
        syscall2(SYS_DUP2, pipefd[0], STDIN_FD);
        syscall1(SYS_CLOSE, pipefd[0]);
        
        exec_simple_command(right_cmd);
        /* Never returns */
    }
    
    /* Parent: close both ends, set foreground, and wait for children */
    syscall1(SYS_CLOSE, pipefd[0]);
    syscall1(SYS_CLOSE, pipefd[1]);
    
    /* Set right_pid as foreground for Ctrl+C (typically the last in pipeline) */
    syscall1(SYS_SETFGPID, right_pid);
    
    int status;
    syscall3(SYS_WAIT, left_pid, (long)&status, 0);
    syscall3(SYS_WAIT, right_pid, (long)&status, 0);
    
    /* Clear foreground process */
    syscall1(SYS_SETFGPID, -1);
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
    
    /* Add to history (before expansion) */
    history_add(g_input_buffer);
    
    /* Expand environment variables */
    env_expand(g_input_buffer, g_expanded_buffer, EXPANDED_CMD_SIZE);
    
    /* Check for pipe */
    int pipe_pos = find_pipe_char(g_expanded_buffer);
    if (pipe_pos >= 0) {
        /* Split command at pipe */
        g_expanded_buffer[pipe_pos] = '\0';
        char *left_cmd = g_expanded_buffer;
        char *right_cmd = &g_expanded_buffer[pipe_pos + 1];
        
        /* Trim whitespace from left command */
        int left_end = pipe_pos - 1;
        while (left_end >= 0 && (left_cmd[left_end] == ' ' || left_cmd[left_end] == '\t')) {
            left_cmd[left_end--] = '\0';
        }
        
        /* Skip leading whitespace on right command */
        while (*right_cmd == ' ' || *right_cmd == '\t') right_cmd++;
        
        execute_pipeline(left_cmd, right_cmd);
        return;
    }
    
    /* Parse for I/O redirections */
    char *input_file = (char *)0;
    char *output_file = (char *)0;
    int append_mode = 0;
    parse_redirections(g_expanded_buffer, &input_file, &output_file, &append_mode);
    
    /* Open input file if redirected */
    long input_fd = -1;
    if (input_file && input_file[0]) {
        input_fd = syscall3(SYS_OPEN, (long)input_file, O_RDONLY, 0);
        if (input_fd < 0) {
            print_string("Error: Cannot open input file\n");
            return;
        }
    }
    
    /* Open output file if redirected */
    long output_fd = -1;
    if (output_file && output_file[0]) {
        int flags = O_WRONLY | O_CREAT;
        if (append_mode) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        output_fd = syscall3(SYS_OPEN, (long)output_file, flags, FILE_MODE);
        if (output_fd < 0) {
            print_string(MSG_REDIR_ERROR);
            return;
        }
    }
    
    /* Recalculate length after redirection parsing */
    int expanded_len = str_length(g_expanded_buffer);
    
    /* Trim trailing whitespace */
    while (expanded_len > 0 && (g_expanded_buffer[expanded_len - 1] == ' ' || 
                                 g_expanded_buffer[expanded_len - 1] == '\t')) {
        expanded_len--;
    }
    g_expanded_buffer[expanded_len] = '\0';
    
    /* Parse command */
    int cmd_len = parse_command_length(g_expanded_buffer, expanded_len);
    int arg_start = parse_arg_start(g_expanded_buffer, expanded_len, cmd_len);
    
    /* Check built-in commands first */
    if (command_matches(g_expanded_buffer, cmd_len, "help")) {
        handle_builtin_help();
    } else if (command_matches(g_expanded_buffer, cmd_len, "exit")) {
        handle_builtin_exit();
    } else if (command_matches(g_expanded_buffer, cmd_len, "echo")) {
        /* Handle echo with optional output redirection */
        if (output_fd >= 0 && arg_start < expanded_len) {
            write_to_fd(output_fd, &g_expanded_buffer[arg_start]);
            write_to_fd(output_fd, NEWLINE);
        } else if (arg_start < expanded_len) {
            print_chars(&g_expanded_buffer[arg_start], expanded_len - arg_start);
            print_string(NEWLINE);
        }
    } else if (command_matches(g_expanded_buffer, cmd_len, "cat")) {
        handle_builtin_cat(arg_start, expanded_len, input_fd);
    } else if (command_matches(g_expanded_buffer, cmd_len, "cd")) {
        handle_builtin_cd(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "mkdir")) {
        handle_builtin_mkdir(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "rmdir")) {
        handle_builtin_rmdir(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "export")) {
        handle_builtin_export(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "unset")) {
        handle_builtin_unset(arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "env")) {
        handle_builtin_env();
    } else if (command_matches(g_expanded_buffer, cmd_len, "history")) {
        handle_builtin_history();
    } else if (command_matches(g_expanded_buffer, cmd_len, "source")) {
        handle_builtin_source(arg_start, expanded_len);
    }
    /* External commands - fork and exec from /bin */
    else if (command_matches(g_expanded_buffer, cmd_len, "ls")) {
        handle_external_command("ls", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "pwd")) {
        handle_external_command("pwd", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "clear")) {
        handle_external_command("clear", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "hello")) {
        handle_external_command("hello", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "clock")) {
        handle_external_command("clock", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "ps")) {
        handle_external_command("ps", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "uname")) {
        handle_external_command("uname", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "uptime")) {
        handle_external_command("uptime", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "whoami")) {
        handle_external_command("whoami", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "tty")) {
        handle_external_command("tty", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "kill")) {
        handle_external_command("kill", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "touch")) {
        handle_external_command("touch", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "rm")) {
        handle_external_command("rm", arg_start, expanded_len);
    } else if (command_matches(g_expanded_buffer, cmd_len, "sleep")) {
        handle_external_command("sleep", arg_start, expanded_len);
    } else {
        print_string(MSG_UNKNOWN_CMD);
    }
    
    /* Close opened file descriptors */
    if (output_fd >= 0) {
        syscall1(SYS_CLOSE, output_fd);
    }
    if (input_fd >= 0) {
        syscall1(SYS_CLOSE, input_fd);
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

/**
 * Handle tab key for filename completion
 */
static void handle_tab(void) {
    if (g_input_pos == 0) {
        return;  /* Nothing to complete */
    }
    
    /* Find the start of the last word (token being typed) */
    int word_start = g_input_pos;
    while (word_start > 0 && g_input_buffer[word_start - 1] != ' ') {
        word_start--;
    }
    
    if (word_start == g_input_pos) {
        return;  /* Empty word */
    }
    
    /* Extract the partial word */
    g_input_buffer[g_input_pos] = '\0';
    const char *partial = &g_input_buffer[word_start];
    int partial_len = g_input_pos - word_start;
    
    /* Determine directory to search and prefix to match */
    char dir_path[PATH_BUFFER_SIZE];
    const char *name_prefix;
    int prefix_len;
    
    /* Check if partial contains a slash (path) */
    int last_slash = -1;
    for (int i = 0; i < partial_len; i++) {
        if (partial[i] == '/') {
            last_slash = i;
        }
    }
    
    if (last_slash >= 0) {
        /* Has path - extract directory */
        if ((unsigned int)last_slash >= sizeof(dir_path) - 1) {
            return;  /* Path too long */
        }
        for (int i = 0; i < last_slash; i++) {
            dir_path[i] = partial[i];
        }
        if (last_slash == 0) {
            dir_path[0] = '/';
            dir_path[1] = '\0';
        } else {
            dir_path[last_slash] = '\0';
        }
        name_prefix = &partial[last_slash + 1];
        prefix_len = partial_len - last_slash - 1;
    } else {
        /* No slash - use current directory */
        dir_path[0] = '.';
        dir_path[1] = '\0';
        name_prefix = partial;
        prefix_len = partial_len;
    }
    
    /* Open the directory */
    long dir_fd = syscall3(SYS_OPEN, (long)dir_path, O_RDONLY, 0);
    if (dir_fd < 0) {
        return;  /* Cannot open directory */
    }
    
    /* Read directory entries and find matches */
    char match_buf[256];
    int match_count = 0;
    const char *first_match = (const char *)0;
    int first_match_len = 0;
    int common_len = 0;  /* Length of common prefix among all matches */
    
    char dirent_buf[512];  /* Buffer for directory entries */
    long bytes_read;
    
    while ((bytes_read = syscall3(SYS_GETDENTS, dir_fd, (long)dirent_buf, sizeof(dirent_buf))) > 0) {
        long pos = 0;
        while (pos < bytes_read) {
            struct thunderos_dirent *de = (struct thunderos_dirent *)&dirent_buf[pos];
            
            /* Check if name starts with our prefix */
            int match = 1;
            if (prefix_len > 0) {
                for (int i = 0; i < prefix_len; i++) {
                    if (de->d_name[i] != name_prefix[i]) {
                        match = 0;
                        break;
                    }
                }
            }
            
            if (match && de->d_name[0] != '\0') {
                /* Skip . and .. */
                if (de->d_name[0] == '.' && 
                    (de->d_name[1] == '\0' || 
                     (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
                    pos += de->d_reclen;
                    continue;
                }
                
                if (match_count == 0) {
                    /* First match - copy it */
                    int i;
                    for (i = 0; de->d_name[i] && i < 255; i++) {
                        match_buf[i] = de->d_name[i];
                    }
                    match_buf[i] = '\0';
                    first_match = match_buf;
                    first_match_len = i;
                    common_len = i;
                } else {
                    /* Find common prefix with existing matches */
                    int i;
                    for (i = 0; i < common_len && de->d_name[i] == match_buf[i]; i++) {
                        /* Keep going while matching */
                    }
                    common_len = i;
                }
                match_count++;
            }
            
            pos += de->d_reclen;
        }
    }
    
    syscall1(SYS_CLOSE, dir_fd);
    
    if (match_count == 0) {
        return;  /* No matches */
    }
    
    if (match_count == 1) {
        /* Single match - complete it fully */
        int chars_to_add = first_match_len - prefix_len;
        for (int i = 0; i < chars_to_add && g_input_pos < INPUT_BUFFER_SIZE - 1; i++) {
            char c = first_match[prefix_len + i];
            g_input_buffer[g_input_pos++] = c;
            print_char(c);
        }
        /* Add trailing slash for directories or space for files */
        /* (We'd need to stat the file to know - for simplicity, just complete) */
    } else if (common_len > prefix_len) {
        /* Multiple matches but can still extend */
        int chars_to_add = common_len - prefix_len;
        for (int i = 0; i < chars_to_add && g_input_pos < INPUT_BUFFER_SIZE - 1; i++) {
            char c = match_buf[prefix_len + i];
            g_input_buffer[g_input_pos++] = c;
            print_char(c);
        }
    }
    /* If multiple matches and no common extension, could beep or show options */
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
    env_init();
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
        } else if (input_char == CHAR_TAB) {
            handle_tab();
        } else if (input_char == CHAR_BACKSPACE || input_char == CHAR_BACKSPACE_ALT) {
            handle_backspace();
        } else if (input_char >= 32 && input_char < 127) {
            handle_printable_char(input_char);
        }
    }
}
