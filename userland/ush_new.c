/*
 * ush - Minimal User Shell for ThunderOS
 * 
 * Clean implementation designed to work with -O0
 */

// System call numbers
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2

// File descriptors
#define STDIN  0
#define STDOUT 1

// External syscall wrappers from syscall.S
extern long syscall0(long n);
extern long syscall1(long n, long a0);
extern long syscall3(long n, long a0, long a1, long a2);

// Simple strlen
static long my_strlen(const char *s) {
    long len = 0;
    while (s[len]) len++;
    return len;
}

// Simple write - inline to avoid stack frame issues
static void my_write(const char *s) {
    long len = my_strlen(s);
    syscall3(SYS_WRITE, STDOUT, (long)s, len);
}

void _start(void) {
    // Initialize global pointer
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
        "   addi gp, gp, %%pcrel_lo(1b)\n"
        ".option pop\n"
        ::: "gp"
    );
    
    // Print banner
    my_write("\n");
    my_write("ThunderOS User Shell v0.5.0\n");
    my_write("\n");
    my_write("Shell running in user mode (PID 1)\n");
    my_write("Type 'exit' to quit\n");
    my_write("\n");
    
    // Main loop - just echo input for now
    char input_buf[256];
    int input_pos = 0;
    
    my_write("ush> ");
    
    while (1) {
        char c;
        long n = syscall3(SYS_READ, STDIN, (long)&c, 1);
        
        if (n != 1) {
            // No input - yield CPU
            syscall0(6); // SYS_YIELD
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            // End of line
            my_write("\n");
            
            // Process command
            input_buf[input_pos] = '\0';
            
            if (input_pos > 0) {
                // Check for exit command
                const char *exit_cmd = "exit";
                int is_exit = 1;
                for (int i = 0; i < 4 && i < input_pos; i++) {
                    if (input_buf[i] != exit_cmd[i]) {
                        is_exit = 0;
                        break;
                    }
                }
                
                if (is_exit && input_pos == 4) {
                    my_write("Goodbye!\n");
                    syscall1(SYS_EXIT, 0);
                }
                
                // Echo back what was typed
                my_write("You typed: ");
                input_buf[input_pos] = '\0';
                my_write(input_buf);
                my_write("\n");
            }
            
            // Reset and show prompt
            input_pos = 0;
            my_write("ush> ");
        }
        else if (c == 127 || c == '\b') {
            // Backspace
            if (input_pos > 0) {
                input_pos--;
                my_write("\b \b");
            }
        }
        else if (c >= 32 && c < 127) {
            // Printable character
            if (input_pos < 255) {
                input_buf[input_pos++] = c;
                // Echo character
                char echo[2];
                echo[0] = c;
                echo[1] = '\0';
                my_write(echo);
            }
        }
    }
}
