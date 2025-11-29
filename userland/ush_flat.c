/*
 * Flat user shell - all logic in _start() to avoid nested stack frames with -O0
 * Features: command history (up/down arrows), basic editing
 */

extern long syscall0(long n);
extern long syscall1(long n, long a0);
extern long syscall2(long n, long a0, long a1);
extern long syscall3(long n, long a0, long a1, long a2);

// History settings
#define HISTORY_SIZE 16
#define MAX_CMD_LEN 256

void _start(void) {
    // Initialize gp
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    // Banner strings
    const char *nl = "\n";
    const char *banner1 = "===========================================\n";
    const char *banner2 = "  ThunderOS User Shell v0.8.0\n";
    const char *banner3 = "===========================================\n";
    const char *info = "Type 'help' for available commands\n";
    const char *prompt = "ush> ";
    
    // Command strings  
    const char *help_response = "Available commands:\n  help  - Show this help\n  echo  - Echo arguments\n  cat   - Display file contents\n  ls    - List directory\n  cd    - Change directory\n  pwd   - Print working directory\n  mkdir - Create directory\n  rmdir - Remove directory\n  clear - Clear screen\n  hello - Run hello program\n  exit  - Exit shell\n";
    const char *goodbye = "Goodbye!\n";
    const char *unknown = "Unknown command (try 'help')\n";
    const char *usage_cat = "Usage: cat <filename>\n";
    const char *usage_echo = "Usage: echo <text>\n";
    const char *usage_cd = "Usage: cd <directory>\n";
    const char *usage_mkdir = "Usage: mkdir <directory>\n";
    const char *usage_rmdir = "Usage: rmdir <directory>\n";
    const char *file_error = "Error: Cannot open file\n";
    const char *cd_error = "cd: cannot access directory\n";
    const char *mkdir_error = "mkdir: cannot create directory\n";
    const char *rmdir_error = "rmdir: cannot remove directory\n";
    
    // Write banner (inline strlen and write)
    const char *s;
    long len;
    
    s = nl; len = 0; while (s[len]) len++; syscall3(1, 1, (long)nl, len);
    s = banner1; len = 0; while (s[len]) len++; syscall3(1, 1, (long)banner1, len);
    s = banner2; len = 0; while (s[len]) len++; syscall3(1, 1, (long)banner2, len);
    s = banner3; len = 0; while (s[len]) len++; syscall3(1, 1, (long)banner3, len);
    s = nl; len = 0; while (s[len]) len++; syscall3(1, 1, (long)nl, len);
    s = info; len = 0; while (s[len]) len++; syscall3(1, 1, (long)info, len);
    s = nl; len = 0; while (s[len]) len++; syscall3(1, 1, (long)nl, len);
    
    // Input buffer
    char input[256];
    int pos = 0;
    
    // Command history
    char history[HISTORY_SIZE][MAX_CMD_LEN];
    int history_count = 0;      // Number of commands in history
    int history_index = 0;      // Current position when browsing
    int browsing_history = 0;   // Flag: are we browsing history?
    
    // Escape sequence state
    int escape_state = 0;       // 0=normal, 1=got ESC, 2=got ESC[
    
    // Initialize history
    for (int i = 0; i < HISTORY_SIZE; i++) {
        history[i][0] = '\0';
    }
    
    // Show initial prompt
    s = prompt; len = 0; while (s[len]) len++; syscall3(1, 1, (long)prompt, len);
    
    // Main loop
    while (1) {
        char c;
        long n = syscall3(2, 0, (long)&c, 1);  // SYS_READ
        
        if (n != 1) {
            syscall0(6);  // SYS_YIELD
            continue;
        }
        
        // Handle escape sequences for arrow keys
        if (escape_state == 1) {
            // Got ESC, expecting '['
            if (c == '[') {
                escape_state = 2;
                continue;
            } else {
                // Not an escape sequence we recognize
                escape_state = 0;
                // Fall through to process 'c' normally
            }
        } else if (escape_state == 2) {
            // Got ESC[, expecting A (up) or B (down)
            escape_state = 0;
            if (c == 'A') {
                // Up arrow - go back in history
                if (history_count > 0) {
                    if (!browsing_history) {
                        // Save current input and start browsing
                        browsing_history = 1;
                        history_index = history_count;
                    }
                    if (history_index > 0) {
                        history_index--;
                        // Clear current line
                        while (pos > 0) {
                            const char *bs = "\b \b";
                            syscall3(1, 1, (long)bs, 3);
                            pos--;
                        }
                        // Copy history entry to input
                        int i = 0;
                        while (history[history_index][i]) {
                            input[i] = history[history_index][i];
                            i++;
                        }
                        input[i] = '\0';
                        pos = i;
                        // Display it
                        syscall3(1, 1, (long)input, pos);
                    }
                }
                continue;
            } else if (c == 'B') {
                // Down arrow - go forward in history
                if (browsing_history && history_index < history_count - 1) {
                    history_index++;
                    // Clear current line
                    while (pos > 0) {
                        const char *bs = "\b \b";
                        syscall3(1, 1, (long)bs, 3);
                        pos--;
                    }
                    // Copy history entry to input
                    int i = 0;
                    while (history[history_index][i]) {
                        input[i] = history[history_index][i];
                        i++;
                    }
                    input[i] = '\0';
                    pos = i;
                    // Display it
                    syscall3(1, 1, (long)input, pos);
                } else if (browsing_history && history_index >= history_count - 1) {
                    // At end of history, clear line
                    browsing_history = 0;
                    while (pos > 0) {
                        const char *bs = "\b \b";
                        syscall3(1, 1, (long)bs, 3);
                        pos--;
                    }
                    input[0] = '\0';
                }
                continue;
            }
            // Not an arrow key, ignore the sequence
            continue;
        }
        
        // Check for ESC (start of escape sequence)
        if (c == 0x1B) {
            escape_state = 1;
            continue;
        }
        
        // Echo character
        if (c == '\r' || c == '\n') {
            // Echo a proper newline sequence
            const char *crlf = "\r\n";
            syscall3(1, 1, (long)crlf, 2);
            
            // End of line
            input[pos] = '\0';
            
            // Reset history browsing state
            browsing_history = 0;
            
            if (pos > 0) {
                // Add command to history (if not empty and different from last)
                int add_to_history = 1;
                if (history_count > 0) {
                    // Check if same as last command
                    int same = 1;
                    int i = 0;
                    while (input[i] && history[history_count - 1][i]) {
                        if (input[i] != history[history_count - 1][i]) {
                            same = 0;
                            break;
                        }
                        i++;
                    }
                    if (input[i] != history[history_count - 1][i]) same = 0;
                    if (same) add_to_history = 0;
                }
                
                if (add_to_history) {
                    // Shift history if full
                    if (history_count >= HISTORY_SIZE) {
                        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                            int j = 0;
                            while (history[i + 1][j]) {
                                history[i][j] = history[i + 1][j];
                                j++;
                            }
                            history[i][j] = '\0';
                        }
                        history_count = HISTORY_SIZE - 1;
                    }
                    // Copy input to history
                    int i = 0;
                    while (input[i] && i < MAX_CMD_LEN - 1) {
                        history[history_count][i] = input[i];
                        i++;
                    }
                    history[history_count][i] = '\0';
                    history_count++;
                }
                
                // Parse command - find first space or end
                int cmd_len = 0;
                while (cmd_len < pos && input[cmd_len] != ' ') cmd_len++;
                
                // Find argument start (skip spaces)
                int arg_start = cmd_len;
                while (arg_start < pos && input[arg_start] == ' ') arg_start++;
                
                // Check commands inline
                int is_help = (cmd_len == 4 && input[0] == 'h' && input[1] == 'e' && input[2] == 'l' && input[3] == 'p');
                int is_exit = (cmd_len == 4 && input[0] == 'e' && input[1] == 'x' && input[2] == 'i' && input[3] == 't');
                int is_echo = (cmd_len == 4 && input[0] == 'e' && input[1] == 'c' && input[2] == 'h' && input[3] == 'o');
                int is_cat = (cmd_len == 3 && input[0] == 'c' && input[1] == 'a' && input[2] == 't');
                int is_hello = (cmd_len == 5 && input[0] == 'h' && input[1] == 'e' && input[2] == 'l' && input[3] == 'l' && input[4] == 'o');
                int is_ls = (cmd_len == 2 && input[0] == 'l' && input[1] == 's');
                int is_cd = (cmd_len == 2 && input[0] == 'c' && input[1] == 'd');
                int is_mkdir = (cmd_len == 5 && input[0] == 'm' && input[1] == 'k' && input[2] == 'd' && input[3] == 'i' && input[4] == 'r');
                int is_rmdir = (cmd_len == 5 && input[0] == 'r' && input[1] == 'm' && input[2] == 'd' && input[3] == 'i' && input[4] == 'r');
                int is_pwd = (cmd_len == 3 && input[0] == 'p' && input[1] == 'w' && input[2] == 'd');
                int is_clear = (cmd_len == 5 && input[0] == 'c' && input[1] == 'l' && input[2] == 'e' && input[3] == 'a' && input[4] == 'r');
                
                if (is_help) {
                    s = help_response;
                    len = 0;
                    while (s[len]) len++;
                    syscall3(1, 1, (long)help_response, len);
                    
                } else if (is_exit) {
                    s = goodbye;
                    len = 0;
                    while (s[len]) len++;
                    syscall3(1, 1, (long)goodbye, len);
                    syscall1(0, 0);  // SYS_EXIT
                    
                } else if (is_echo) {
                    if (arg_start < pos) {
                        // Echo the arguments
                        int arg_len = pos - arg_start;
                        syscall3(1, 1, (long)&input[arg_start], arg_len);
                        s = nl;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)nl, len);
                    } else {
                        s = usage_echo;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)usage_echo, len);
                    }
                    
                } else if (is_cat) {
                    if (arg_start < pos) {
                        // Null-terminate the filename
                        input[pos] = '\0';
                        
                        // Open file (SYS_OPEN = 13, O_RDONLY = 0)
                        long fd = syscall3(13, (long)&input[arg_start], 0, 0);
                        if (fd >= 0) {
                            // Read and display file contents
                            char buf[256];
                            long nread;
                            while ((nread = syscall3(2, fd, (long)buf, 255)) > 0) {
                                syscall3(1, 1, (long)buf, nread);
                            }
                            // Close file (SYS_CLOSE = 14)
                            syscall1(14, fd);
                        } else {
                            s = file_error;
                            len = 0;
                            while (s[len]) len++;
                            syscall3(1, 1, (long)file_error, len);
                        }
                    } else {
                        s = usage_cat;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)usage_cat, len);
                    }
                    
                } else if (is_hello) {
                    // Fork and exec hello program
                    long pid = syscall0(7);  // SYS_FORK
                    if (pid == 0) {
                        // Child process - exec /bin/hello
                        const char *path = "/bin/hello";
                        const char *argv[] = { path, 0 };
                        const char *envp[] = { 0 };
                        syscall3(20, (long)path, (long)argv, (long)envp);  // SYS_EXECVE
                        // If exec fails, exit
                        const char *exec_fail = "Failed to exec hello\n";
                        s = exec_fail;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)exec_fail, len);
                        syscall1(0, 1);
                    } else if (pid > 0) {
                        // Parent - wait for child (SYS_WAIT = 9)
                        int status;
                        syscall3(9, pid, (long)&status, 0);
                    }
                    
                } else if (is_ls) {
                    // Fork and exec ls program
                    long pid = syscall0(7);  // SYS_FORK
                    if (pid == 0) {
                        // Child process - exec /bin/ls
                        const char *path = "/bin/ls";
                        const char *argv[] = { path, 0 };
                        const char *envp[] = { 0 };
                        syscall3(20, (long)path, (long)argv, (long)envp);  // SYS_EXECVE
                        // If exec fails, exit
                        const char *exec_fail = "Failed to exec ls\n";
                        s = exec_fail;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)exec_fail, len);
                        syscall1(0, 1);
                    } else if (pid > 0) {
                        // Parent - wait for child (SYS_WAIT = 9)
                        int status;
                        syscall3(9, pid, (long)&status, 0);
                    }
                    
                } else if (is_cd) {
                    // cd is a shell built-in (changes shell's cwd)
                    if (arg_start < pos) {
                        // Null-terminate the path
                        input[pos] = '\0';
                        // SYS_CHDIR = 28
                        long result = syscall1(28, (long)&input[arg_start]);
                        if (result != 0) {
                            s = cd_error;
                            len = 0;
                            while (s[len]) len++;
                            syscall3(1, 1, (long)cd_error, len);
                        }
                    } else {
                        s = usage_cd;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)usage_cd, len);
                    }
                    
                } else if (is_mkdir) {
                    // mkdir is a shell built-in
                    if (arg_start < pos) {
                        // Null-terminate the path
                        input[pos] = '\0';
                        // SYS_MKDIR = 17, mode = 0755
                        long result = syscall2(17, (long)&input[arg_start], 0755);
                        if (result != 0) {
                            s = mkdir_error;
                            len = 0;
                            while (s[len]) len++;
                            syscall3(1, 1, (long)mkdir_error, len);
                        }
                    } else {
                        s = usage_mkdir;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)usage_mkdir, len);
                    }
                    
                } else if (is_rmdir) {
                    // rmdir is a shell built-in
                    if (arg_start < pos) {
                        // Null-terminate the path
                        input[pos] = '\0';
                        // SYS_RMDIR = 19
                        long result = syscall1(19, (long)&input[arg_start]);
                        if (result != 0) {
                            s = rmdir_error;
                            len = 0;
                            while (s[len]) len++;
                            syscall3(1, 1, (long)rmdir_error, len);
                        }
                    } else {
                        s = usage_rmdir;
                        len = 0;
                        while (s[len]) len++;
                        syscall3(1, 1, (long)usage_rmdir, len);
                    }
                    
                } else if (is_pwd) {
                    // pwd - fork and exec /bin/pwd
                    long pid = syscall0(7);  // SYS_FORK
                    if (pid == 0) {
                        const char *path = "/bin/pwd";
                        const char *argv[] = { path, 0 };
                        const char *envp[] = { 0 };
                        syscall3(20, (long)path, (long)argv, (long)envp);
                        syscall1(0, 1);
                    } else if (pid > 0) {
                        int status;
                        syscall3(9, pid, (long)&status, 0);
                    }
                    
                } else if (is_clear) {
                    // clear - fork and exec /bin/clear
                    long pid = syscall0(7);  // SYS_FORK
                    if (pid == 0) {
                        const char *path = "/bin/clear";
                        const char *argv[] = { path, 0 };
                        const char *envp[] = { 0 };
                        syscall3(20, (long)path, (long)argv, (long)envp);
                        syscall1(0, 1);
                    } else if (pid > 0) {
                        int status;
                        syscall3(9, pid, (long)&status, 0);
                    }
                    
                } else {
                    s = unknown;
                    len = 0;
                    while (s[len]) len++;
                    syscall3(1, 1, (long)unknown, len);
                }
            }
            
            // Reset and show prompt
            pos = 0;
            s = prompt;
            len = 0;
            while (s[len]) len++;
            syscall3(1, 1, (long)prompt, len);
        }
        else if (c == 127 || c == '\b') {
            // Backspace
            if (pos > 0) {
                pos--;
                const char *bs = "\b \b";
                syscall3(1, 1, (long)bs, 3);
            }
        }
        else if (c >= 32 && c < 127) {
            // Printable character - echo and store
            syscall3(1, 1, (long)&c, 1);
            if (pos < 255) {
                input[pos++] = c;
            }
        }
    }
}
