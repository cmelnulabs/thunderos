/*
 * Signal handling for ThunderOS
 * 
 * Implements POSIX-style signals for process communication and control.
 */

#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>

// Forward declaration
struct process;
struct trap_frame;

// Signal numbers
#define SIGHUP      1   // Hangup
#define SIGINT      2   // Interrupt (Ctrl+C)
#define SIGQUIT     3   // Quit
#define SIGILL      4   // Illegal instruction
#define SIGTRAP     5   // Trace/breakpoint trap
#define SIGABRT     6   // Aborted
#define SIGBUS      7   // Bus error
#define SIGFPE      8   // Floating point exception
#define SIGKILL     9   // Kill (cannot be caught or ignored)
#define SIGUSR1     10  // User-defined signal 1
#define SIGSEGV     11  // Segmentation fault
#define SIGUSR2     12  // User-defined signal 2
#define SIGPIPE     13  // Broken pipe
#define SIGALRM     14  // Alarm clock
#define SIGTERM     15  // Termination signal
#define SIGCHLD     17  // Child process state changed
#define SIGCONT     18  // Continue if stopped
#define SIGSTOP     19  // Stop process (cannot be caught or ignored)
#define SIGTSTP     20  // Stop from terminal (Ctrl+Z)
#define SIGTTIN     21  // Background read from terminal
#define SIGTTOU     22  // Background write to terminal

#define NSIG        32  // Total number of signals

// Signal handler types
#define SIG_DFL     ((void (*)(int))0)  // Default handler
#define SIG_IGN     ((void (*)(int))1)  // Ignore signal
#define SIG_ERR     ((void (*)(int))-1) // Error return

// Signal set type (bitmask)
typedef uint64_t sigset_t;

// Signal handler function type
typedef void (*sighandler_t)(int);

// Signal action structure
struct sigaction {
    sighandler_t sa_handler;    // Signal handler function
    sigset_t sa_mask;           // Signals to block during handler
    int sa_flags;               // Special flags
};

// sigaction flags
#define SA_NOCLDSTOP    1       // Don't notify on child stop
#define SA_NOCLDWAIT    2       // Don't create zombie on child death
#define SA_SIGINFO      4       // Use sa_sigaction instead of sa_handler
#define SA_RESTART      8       // Restart syscalls if interrupted

/**
 * Initialize signal subsystem for a process
 * 
 * @param proc Process to initialize signals for
 */
void signal_init_process(struct process *proc);

/**
 * Send a signal to a process
 * 
 * @param proc Target process
 * @param signum Signal number
 * @return 0 on success, -1 on error
 */
int signal_send(struct process *proc, int signum);

/**
 * Check and deliver pending signals
 * 
 * Called from trap handler before returning to user mode.
 * 
 * @param proc Process to check signals for
 */
void signal_deliver(struct process *proc);

/**
 * Check and deliver pending signals with trap frame
 * 
 * Called from trap handler before returning to user mode with trap frame for handler execution.
 * 
 * @param proc Process to check signals for
 * @param trap_frame Trap frame to modify for signal handler execution  
 */
void signal_deliver_with_frame(struct process *proc, struct trap_frame *trap_frame);

/**
 * Handle a signal for a process
 * 
 * @param proc Process receiving signal
 * @param signum Signal number
 */
void signal_handle(struct process *proc, int signum);

/**
 * Handle a signal with trap frame
 * 
 * @param proc Process receiving signal
 * @param signum Signal number
 * @param trap_frame Trap frame for user handler execution
 */
void signal_handle_with_frame(struct process *proc, int signum, struct trap_frame *trap_frame);

/**
 * Set signal handler
 * 
 * @param proc Process to set handler for
 * @param signum Signal number
 * @param handler Handler function (SIG_DFL or SIG_IGN or user function)
 * @return Previous handler, or SIG_ERR on error
 */
sighandler_t signal_set_handler(struct process *proc, int signum, sighandler_t handler);

/**
 * Check if signal is pending
 * 
 * @param proc Process to check
 * @param signum Signal number
 * @return 1 if pending, 0 otherwise
 */
int signal_is_pending(struct process *proc, int signum);

/**
 * Default signal handlers
 */
void signal_default_term(struct process *proc);  // Terminate
void signal_default_ignore(struct process *proc); // Ignore
void signal_default_stop(struct process *proc);  // Stop
void signal_default_cont(struct process *proc);  // Continue

/**
 * Get default action for a signal
 * 
 * @param signum Signal number
 * @return Default handler function
 */
sighandler_t signal_default_action(int signum);

#endif // SIGNAL_H
