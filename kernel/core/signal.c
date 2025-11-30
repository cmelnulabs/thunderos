/*
 * Signal handling implementation for ThunderOS
 */

#include "kernel/signal.h"
#include "kernel/process.h"
#include "kernel/errno.h"
#include "kernel/kstring.h"
#include "hal/hal_uart.h"

// Exit code when process terminated by signal (128 + signal number convention)
#define SIGNAL_EXIT_BASE 128

// External process functions
extern struct process *process_current(void);
extern void process_exit(int exit_code);

/**
 * Initialize signal subsystem for a process
 * 
 * Sets all signal handlers to SIG_DFL and clears pending/blocked signal masks.
 * Must be called when creating a new process.
 * 
 * @param proc Process to initialize (must not be NULL)
 */
void signal_init_process(struct process *proc) {
    if (!proc) {
        return;
    }
    
    proc->pending_signals = 0;
    proc->blocked_signals = 0;
    
    // Set all handlers to default
    for (int i = 0; i < NSIG; i++) {
        proc->signal_handlers[i] = SIG_DFL;
    }
}

/**
 * Get default action for a signal
 */
sighandler_t signal_default_action(int signum) {
    switch (signum) {
        // Signals that terminate the process
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGILL:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGKILL:
        case SIGSEGV:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
            return SIG_DFL;  // Will use signal_default_term
            
        // Signals that stop the process
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return SIG_DFL;  // Will use signal_default_stop
            
        // Signals that continue the process
        case SIGCONT:
            return SIG_DFL;  // Will use signal_default_cont
            
        // Signals that are ignored by default
        case SIGCHLD:
            return SIG_IGN;
            
        default:
            return SIG_DFL;
    }
}

/**
 * Send a signal to a process
 * 
 * Marks the signal as pending for the target process. Actual delivery
 * happens when returning to user mode from a trap.
 * 
 * @param proc Target process (must not be NULL)
 * @param signum Signal number (1 to NSIG-1)
 * @return 0 on success, -1 on error (sets errno)
 */
int signal_send(struct process *proc, int signum) {
    if (!proc || signum <= 0 || signum >= NSIG) {
        set_errno(THUNDEROS_EINVAL);
        return -1;
    }
    
    // Can't send signals to UNUSED or ZOMBIE processes
    if (proc->state == PROC_UNUSED || proc->state == PROC_ZOMBIE) {
        set_errno(THUNDEROS_ESRCH);  // No such process
        return -1;
    }
    
    // Add signal to pending set
    proc->pending_signals |= (1UL << signum);
    
    // Wake up the process if it's sleeping (for most signals)
    if (proc->state == PROC_SLEEPING && signum != SIGCONT) {
        extern void process_wakeup(struct process *proc);
        process_wakeup(proc);
    }
    
    clear_errno();
    return 0;
}

/**
 * Check if signal is pending
 */
int signal_is_pending(struct process *proc, int signum) {
    if (!proc || signum <= 0 || signum >= NSIG) {
        return 0;
    }
    
    return (proc->pending_signals & (1UL << signum)) != 0;
}

/**
 * Set signal handler
 */
sighandler_t signal_set_handler(struct process *proc, int signum, sighandler_t handler) {
    if (!proc || signum <= 0 || signum >= NSIG) {
        set_errno(THUNDEROS_EINVAL);
        return SIG_ERR;
    }
    
    // SIGKILL and SIGSTOP cannot be caught or ignored
    if (signum == SIGKILL || signum == SIGSTOP) {
        set_errno(THUNDEROS_EINVAL);
        return SIG_ERR;
    }
    
    sighandler_t old_handler = proc->signal_handlers[signum];
    proc->signal_handlers[signum] = handler;
    
    clear_errno();
    return old_handler;
}

/**
 * Default signal handler: terminate process
 * 
 * @param proc Process to terminate (unused, uses current process)
 */
void signal_default_term(struct process *proc) {
    (void)proc;  // Unused - process_exit operates on current process
    // Terminate the process with signal exit code
    process_exit(SIGNAL_EXIT_BASE);  // Exit with signal indicator
}

/**
 * Default signal handler: ignore
 * 
 * @param proc Process (unused)
 */
void signal_default_ignore(struct process *proc) {
    (void)proc;  // Intentionally ignored
    // Do nothing - signal is ignored
}

/**
 * Default signal handler: stop process
 * 
 * Changes process state to STOPPED and notifies parent with SIGCHLD.
 * Used for SIGTSTP (Ctrl+Z), SIGSTOP, SIGTTIN, SIGTTOU.
 * 
 * @param proc Process to stop
 */
void signal_default_stop(struct process *proc) {
    if (!proc) return;
    
    // Change state to STOPPED
    proc->state = PROC_STOPPED;
    
    // Store exit status with stop signal info (for waitpid)
    // Upper byte = signal number, lower byte = 0x7f indicates stopped
    proc->exit_code = (SIGTSTP << 8) | 0x7f;
    
    // Notify parent with SIGCHLD so it can detect the stop
    if (proc->parent) {
        signal_send(proc->parent, SIGCHLD);
        // Wake parent if it's waiting
        if (proc->parent->state == PROC_SLEEPING) {
            extern void process_wakeup(struct process *p);
            process_wakeup(proc->parent);
        }
    }
}

/**
 * Default signal handler: continue process
 * 
 * Wakes up a stopped process by changing state from STOPPED to READY.
 * Used for SIGCONT to resume a process stopped by Ctrl+Z.
 * 
 * @param proc Process to continue
 */
void signal_default_cont(struct process *proc) {
    if (!proc) return;
    
    // If process was stopped, wake it up
    if (proc->state == PROC_STOPPED) {
        extern void process_wakeup(struct process *p);
        process_wakeup(proc);
    }
}

/**
 * Handle a signal for a process (without trap frame)
 * 
 * Uses proc->trap_frame for handler execution. For signals delivered
 * from trap handler, use signal_handle_with_frame() instead.
 * 
 * @param proc Process receiving signal
 * @param signum Signal number to handle
 */
void signal_handle(struct process *proc, int signum) {
    if (!proc || signum <= 0 || signum >= NSIG) {
        return;
    }
    
    sighandler_t handler = proc->signal_handlers[signum];
    
    // Use default action if handler is SIG_DFL
    if (handler == SIG_DFL) {
        switch (signum) {
            case SIGKILL:
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
            case SIGILL:
            case SIGABRT:
            case SIGBUS:
            case SIGFPE:
            case SIGSEGV:
            case SIGPIPE:
            case SIGALRM:
            case SIGHUP:
            case SIGUSR1:
            case SIGUSR2:
                signal_default_term(proc);
                break;
                
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                signal_default_stop(proc);
                break;
                
            case SIGCONT:
                signal_default_cont(proc);
                break;
                
            case SIGCHLD:
            default:
                signal_default_ignore(proc);
                break;
        }
    } else if (handler == SIG_IGN) {
        // Ignore the signal
        return;
    } else {
        // User-defined handler - modify trap frame to execute handler
        // For now, we can't execute user handlers without a trap frame
        // They will only be executed when signal_handle_with_frame is called
    }
}

/**
 * Handle a signal with trap frame for user handler execution
 * 
 * Executes the signal handler by either calling default actions (terminate,
 * stop, continue, ignore) or redirecting execution to user-defined handlers
 * by modifying the trap frame.
 * 
 * @param proc Process receiving the signal
 * @param signum Signal number to handle
 * @param trap_frame Trap frame to modify for user handler execution (can be NULL)
 */
void signal_handle_with_frame(struct process *proc, int signum, struct trap_frame *trap_frame) {
    if (!proc || signum <= 0 || signum >= NSIG) {
        return;
    }
    
    sighandler_t handler = proc->signal_handlers[signum];
    
    // Use default action if handler is SIG_DFL
    if (handler == SIG_DFL) {
        switch (signum) {
            case SIGKILL:
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
            case SIGILL:
            case SIGABRT:
            case SIGBUS:
            case SIGFPE:
            case SIGSEGV:
            case SIGPIPE:
            case SIGALRM:
            case SIGHUP:
            case SIGUSR1:
            case SIGUSR2:
                signal_default_term(proc);
                break;
                
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                signal_default_stop(proc);
                break;
                
            case SIGCONT:
                signal_default_cont(proc);
                break;
                
            case SIGCHLD:
            default:
                signal_default_ignore(proc);
                break;
        }
    } else if (handler == SIG_IGN) {
        // Ignore the signal
        return;
    } else {
        // User-defined handler - modify trap frame to execute handler
        if (trap_frame) {
            // Save the return address: when handler executes 'ret', 
            // it will return to the interrupted instruction
            trap_frame->ra = trap_frame->sepc;
            
            // Redirect execution to the signal handler
            trap_frame->sepc = (unsigned long)handler;
            trap_frame->a0 = signum;  // Pass signal number as argument
            
            // TODO: Implement full signal frame with sigreturn for proper context restoration
            // This simple approach works for handlers that end with 'ret'
        }
    }
}


/**
 * Deliver pending signals to a process with trap frame
 * 
 * Called from trap handler before returning to user mode. Checks for
 * unblocked pending signals and delivers one signal per call by modifying
 * the trap frame to execute the signal handler.
 * 
 * @param proc Process to check for signals
 * @param trap_frame Trap frame to modify for handler execution
 */
void signal_deliver_with_frame(struct process *proc, struct trap_frame *trap_frame) {
    if (!proc || !trap_frame) {
        return;
    }
    
    // Get unblocked pending signals
    sigset_t deliverable = proc->pending_signals & ~proc->blocked_signals;
    
    if (!deliverable) {
        return;  // No signals to deliver
    }
    
    // Deliver signals in order (lowest number first)
    for (int signum = 1; signum < NSIG; signum++) {
        if (deliverable & (1UL << signum)) {
            // Clear the pending bit
            proc->pending_signals &= ~(1UL << signum);
            
            // Handle the signal with the trap frame
            signal_handle_with_frame(proc, signum, trap_frame);
            
            // Only deliver one signal at a time
            // (the handler might have changed the process state)
            break;
        }
    }
}

/**
 * Deliver pending signals to a process (original version)
 * 
 * Uses proc->trap_frame for handler execution. For trap handler use,
 * prefer signal_deliver_with_frame() with the current trap frame.
 * 
 * @param proc Process to check for signals
 */
void signal_deliver(struct process *proc) {
    if (!proc) {
        return;
    }
    
    // Get unblocked pending signals
    sigset_t deliverable = proc->pending_signals & ~proc->blocked_signals;
    
    if (!deliverable) {
        return;  // No signals to deliver
    }
    
    // Deliver signals in order (lowest number first)
    for (int signum = 1; signum < NSIG; signum++) {
        if (deliverable & (1UL << signum)) {
            // Clear the pending bit
            proc->pending_signals &= ~(1UL << signum);
            
            // Handle the signal (without trap frame - will use proc->trap_frame)
            signal_handle_with_frame(proc, signum, proc->trap_frame);
            
            // Only deliver one signal at a time
            // (the handler might have changed the process state)
            break;
        }
    }
}
