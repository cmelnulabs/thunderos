/*
 * Flat user shell - all logic in _start() to avoid nested stack frames with -O0
 */

extern long syscall0(long n);
extern long syscall1(long n, long a0);
extern long syscall2(long n, long a0, long a1);
extern long syscall3(long n, long a0, long a1, long a2);

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
    const char *banner2 = "  ThunderOS User Shell v0.6.0\n";
    const char *banner3 = "===========================================\n";
    const char *info = "Type 'help' for available commands\n";
    const char *prompt = "ush> ";
    
    // Command strings  
    const char *help_response = "Available commands:\n  help  - Show this help\n  echo  - Echo arguments\n  cat   - Display file contents\n  hello - Run hello program\n  exit  - Exit shell\n";
    const char *goodbye = "Goodbye!\n";
    const char *unknown = "Unknown command (try 'help')\n";
    const char *usage_cat = "Usage: cat <filename>\n";
    const char *usage_echo = "Usage: echo <text>\n";
    const char *file_error = "Error: Cannot open file\n";
    
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
        
        // Echo character
        syscall3(1, 1, (long)&c, 1);
        
        if (c == '\r' || c == '\n') {
            // End of line
            input[pos] = '\0';
            
            if (pos > 0) {
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
            // Printable character
            if (pos < 255) {
                input[pos++] = c;
            }
        }
    }
}
