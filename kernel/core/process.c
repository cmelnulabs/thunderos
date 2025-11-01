/*
 * Process Management Implementation
 */

#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/kstring.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "hal/hal_uart.h"
#include <stddef.h>

// Process table
static struct process process_table[MAX_PROCS];

// Current running process
static struct process *current_process = NULL;

// Next PID to allocate
static pid_t next_pid = 1;

// Lock for process table (simple spinlock)
static volatile int process_lock = 0;

// Simple spinlock functions
static inline void lock_acquire(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        // Spin
    }
}

static inline void lock_release(volatile int *lock) {
    __sync_lock_release(lock);
}

/**
 * Initialize the process management subsystem
 */
void process_init(void) {
    // Initialize process table
    for (int i = 0; i < MAX_PROCS; i++) {
        process_table[i].state = PROC_UNUSED;
        process_table[i].pid = -1;
    }
    
    // Create the initial kernel process (process 0)
    struct process *init_proc = &process_table[0];
    init_proc->pid = 0;
    init_proc->state = PROC_RUNNING;
    kstrcpy(init_proc->name, "init");
    init_proc->page_table = get_kernel_page_table();
    init_proc->kernel_stack = 0;  // Uses boot stack
    init_proc->user_stack = 0;
    init_proc->cpu_time = 0;
    init_proc->priority = 0;
    init_proc->parent = NULL;
    init_proc->exit_code = 0;
    init_proc->trap_frame = NULL;
    
    current_process = init_proc;
    
    hal_uart_puts("[OK] Process management initialized\n");
}

/**
 * Get the currently running process
 */
struct process *process_current(void) {
    return current_process;
}

/**
 * Set the current process (used by scheduler)
 */
void process_set_current(struct process *proc) {
    current_process = proc;
}

/**
 * Allocate a new PID
 */
pid_t alloc_pid(void) {
    lock_acquire(&process_lock);
    pid_t pid = next_pid++;
    lock_release(&process_lock);
    return pid;
}

/**
 * Find a free process slot
 */
static struct process *alloc_process(void) {
    lock_acquire(&process_lock);
    
    for (int i = 0; i < MAX_PROCS; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            process_table[i].state = PROC_EMBRYO;
            lock_release(&process_lock);
            return &process_table[i];
        }
    }
    
    lock_release(&process_lock);
    return NULL;
}

/**
 * Get process by PID
 */
struct process *process_get(pid_t pid) {
    for (int i = 0; i < MAX_PROCS; i++) {
        if (process_table[i].pid == pid && process_table[i].state != PROC_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

/**
 * Free a process structure
 */
void process_free(struct process *proc) {
    if (!proc) return;
    
    lock_acquire(&process_lock);
    
    // Free kernel stack
    if (proc->kernel_stack) {
        kfree((void *)proc->kernel_stack);
    }
    
    // Free user stack
    if (proc->user_stack) {
        kfree((void *)proc->user_stack);
    }
    
    // Free trap frame
    if (proc->trap_frame) {
        kfree(proc->trap_frame);
    }
    
    // Free page table (not kernel page table)
    if (proc->page_table && proc->page_table != get_kernel_page_table()) {
        // TODO: Walk page table and free all pages
        kfree(proc->page_table);
    }
    
    // Mark as unused
    proc->state = PROC_UNUSED;
    proc->pid = -1;
    
    lock_release(&process_lock);
}

/**
 * Helper to setup initial trap frame for new process
 */
static void setup_trap_frame(struct process *proc, void (*entry_point)(void *), void *arg) {
    // Allocate trap frame separately (not on the stack to avoid corruption)
    proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!proc->trap_frame) {
        return;  // Caller will check and handle
    }
    
    // Zero out trap frame
    kmemset(proc->trap_frame, 0, sizeof(struct trap_frame));
    
    // Set up initial register values
    proc->trap_frame->sepc = (unsigned long)entry_point;  // Entry point
    proc->trap_frame->sp = proc->user_stack + USER_STACK_SIZE;  // User stack pointer
    proc->trap_frame->a0 = (unsigned long)arg;  // First argument
    
    // Set sstatus: SPP=1 (supervisor mode), SPIE=1 (enable interrupts on sret)
    // For kernel processes, we run in supervisor mode
    proc->trap_frame->sstatus = (1 << 8) | (1 << 5);  // SPP=1, SPIE=1
}

/**
 * Wrapper function that calls the process entry point and handles exit
 */
static void process_wrapper(void) {
    struct process *proc = process_current();
    if (!proc || !proc->trap_frame) {
        hal_uart_puts("Error: No process or trap frame in process_wrapper\n");
        while (1) __asm__ volatile("wfi");
    }
    
    // Get entry point and argument from trap frame
    void (*entry_point)(void *) = (void (*)(void *))proc->trap_frame->sepc;
    void *arg = (void *)proc->trap_frame->a0;
    
    // Call the actual process function
    entry_point(arg);
    
    // If process returns, exit
    process_exit(0);
}

/**
 * Create a new process
 */
struct process *process_create(const char *name, void (*entry_point)(void *), void *arg) {
    struct process *proc = alloc_process();
    if (!proc) {
        hal_uart_puts("Failed to allocate process\n");
        return NULL;
    }
    
    // Assign PID
    proc->pid = alloc_pid();
    
    // Copy name
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Allocate kernel stack
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        hal_uart_puts("Failed to allocate kernel stack\n");
        process_free(proc);
        return NULL;
    }
    
    // For now, create kernel-mode processes only (use kernel page table)
    // TODO: Create user-mode processes with separate page tables
    proc->page_table = get_kernel_page_table();
    
    // Allocate user stack (in kernel space for now)
    proc->user_stack = (uintptr_t)kmalloc(USER_STACK_SIZE);
    if (!proc->user_stack) {
        hal_uart_puts("Failed to allocate user stack\n");
        process_free(proc);
        return NULL;
    }
    
    // Setup initial trap frame
    setup_trap_frame(proc, entry_point, arg);
    if (!proc->trap_frame) {
        hal_uart_puts("Failed to allocate trap frame\n");
        process_free(proc);
        return NULL;
    }
    
    // Initialize context (kernel context for context switching)
    kmemset(&proc->context, 0, sizeof(struct context));
    // Set return address to wrapper function
    proc->context.ra = (unsigned long)process_wrapper;
    // Set stack pointer to top of kernel stack (grows downward)
    // Subtract STACK_ALIGNMENT to ensure 16-byte alignment per RISC-V ABI
    proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // Initialize other fields
    proc->cpu_time = 0;
    proc->priority = 10;  // Default priority
    proc->parent = current_process;
    proc->exit_code = 0;
    
    // Mark as ready
    proc->state = PROC_READY;
    
    // Add to scheduler queue
    scheduler_enqueue(proc);
    
    return proc;
}

/**
 * Exit the current process
 */
void process_exit(int exit_code) {
    struct process *proc = current_process;
    
    if (!proc || proc->pid == 0) {
        // Can't exit init process
        hal_uart_puts("Cannot exit init process\n");
        while (1) {
            __asm__ volatile("wfi");
        }
    }
    
    lock_acquire(&process_lock);
    
    proc->state = PROC_ZOMBIE;
    proc->exit_code = exit_code;
    
    lock_release(&process_lock);
    
    // Schedule another process
    process_yield();
    
    // Should never reach here
    while (1) {
        __asm__ volatile("wfi");
    }
}

/**
 * Yield CPU to another process
 */
void process_yield(void) {
    extern void schedule(void);
    schedule();
}

/**
 * Sleep for a number of ticks
 */
void process_sleep(uint64_t ticks) {
    struct process *proc = current_process;
    
    if (!proc) return;
    
    lock_acquire(&process_lock);
    proc->state = PROC_SLEEPING;
    // TODO: Add to sleep queue with wakeup time
    (void)ticks;  // Unused until timer queue is implemented
    lock_release(&process_lock);
    
    process_yield();
}

/**
 * Wake up a process
 */
void process_wakeup(struct process *proc) {
    if (!proc) return;
    
    lock_acquire(&process_lock);
    if (proc->state == PROC_SLEEPING) {
        proc->state = PROC_READY;
        scheduler_enqueue(proc);
    }
    lock_release(&process_lock);
}

/**
 * Dump process table (for debugging)
 */
void process_dump(void) {
    hal_uart_puts("\n=== Process Table ===\n");
    hal_uart_puts("PID  State     Name\n");
    hal_uart_puts("---  --------  --------\n");
    
    for (int i = 0; i < MAX_PROCS; i++) {
        struct process *p = &process_table[i];
        if (p->state != PROC_UNUSED) {
            // Print PID
            kprint_dec(p->pid);
            hal_uart_puts("    ");
            
            // Print state
            const char *state_str = "UNKNOWN";
            switch (p->state) {
                case PROC_UNUSED: state_str = "UNUSED"; break;
                case PROC_EMBRYO: state_str = "EMBRYO"; break;
                case PROC_READY: state_str = "READY"; break;
                case PROC_RUNNING: state_str = "RUNNING"; break;
                case PROC_SLEEPING: state_str = "SLEEPING"; break;
                case PROC_ZOMBIE: state_str = "ZOMBIE"; break;
            }
            hal_uart_puts(state_str);
            hal_uart_puts("  ");
            
            // Print name
            hal_uart_puts(p->name);
            hal_uart_puts("\n");
        }
    }
    hal_uart_puts("\n");
}

/**
 * Fork the current process
 * TODO: Implement fork
 */
pid_t process_fork(void) {
    hal_uart_puts("process_fork: not yet implemented\n");
    return -1;
}

/**
 * Execute a new program in the current process
 * TODO: Implement exec
 */
int process_exec(void (*entry_point)(void *), void *arg) {
    hal_uart_puts("process_exec: not yet implemented\n");
    (void)entry_point;
    (void)arg;
    return -1;
}
