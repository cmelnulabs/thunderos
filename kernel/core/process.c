/*
 * Process Management Implementation
 */

#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/kstring.h"
#include "kernel/panic.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "mm/paging.h"
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
 * Uses atomic increment to ensure no PID conflicts in multi-threaded context
 */
pid_t alloc_pid(void) {
    lock_acquire(&process_lock);
    pid_t pid = next_pid++;
    lock_release(&process_lock);
    return pid;
}

/**
 * Find a free process slot in the process table
 * 
 * Scans the process table for an unused slot and returns it.
 * The process state must be PROC_UNUSED.
 * 
 * @return Pointer to unused process structure, or NULL if table full
 */
static struct process *alloc_process(void) {
    lock_acquire(&process_lock);
    
    for (int i = 0; i < MAX_PROCS; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            // Mark slot as being allocated to prevent race conditions
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
 * Free a process structure and all its resources
 * 
 * Frees kernel stack, user stack, trap frame, and user page table.
 * Does NOT free the kernel page table (shared by all processes).
 * Caller must ensure process is not currently running.
 * 
 * @param proc Process to free (NULL is safe)
 */
void process_free(struct process *proc) {
    if (!proc) return;
    
    lock_acquire(&process_lock);
    
    // Free allocated memory regions
    if (proc->kernel_stack) {
        kfree((void *)proc->kernel_stack);
    }
    
    if (proc->user_stack) {
        kfree((void *)proc->user_stack);
    }
    
    if (proc->trap_frame) {
        kfree(proc->trap_frame);
    }
    
    // Free user page table (but NOT the shared kernel page table)
    if (proc->page_table && proc->page_table != get_kernel_page_table()) {
        free_page_table(proc->page_table);
    }
    
    // Mark process slot as unused
    proc->state = PROC_UNUSED;
    proc->pid = -1;
    
    lock_release(&process_lock);
}

/**
 * Setup initial trap frame for new kernel process
 * 
 * Initializes a trap frame with the entry point and first argument.
 * For kernel processes, sets SPP=1 (supervisor mode) and SPIE=1.
 * 
 * @param proc Process to setup
 * @param entry_point Entry point function pointer
 * @param arg First argument to entry point
 */
static void setup_trap_frame(struct process *proc, void (*entry_point)(void *), void *arg) {
    // Allocate trap frame separately (stack allocation would cause corruption)
    proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!proc->trap_frame) {
        kernel_panic("setup_trap_frame: failed to allocate trap frame");
    }
    
    // Zero out trap frame structure
    kmemset(proc->trap_frame, 0, sizeof(struct trap_frame));
    
    // Set entry point (execution will start here after context switch)
    proc->trap_frame->sepc = (unsigned long)entry_point;
    
    // Set stack pointer to top of user stack (grows downward)
    proc->trap_frame->sp = proc->user_stack + USER_STACK_SIZE;
    
    // Set first argument for entry point function
    proc->trap_frame->a0 = (unsigned long)arg;
    
    // Set sstatus for supervisor mode with interrupts enabled
    // SPP=1 (supervisor mode), SPIE=1 (enable interrupts after sret)
    proc->trap_frame->sstatus = (1 << 8) | (1 << 5);
}

/**
 * Process wrapper function called on first context switch
 * 
 * Called when a new kernel process is scheduled for the first time.
 * Extracts the entry point and argument from the trap frame, calls the
 * actual process function, and calls process_exit if the function returns.
 */
static void process_wrapper(void) {
    struct process *proc = process_current();
    if (!proc || !proc->trap_frame) {
        hal_uart_puts("Error: No process or trap frame in process_wrapper\n");
        while (1) __asm__ volatile("wfi");
    }
    
    // Extract entry point and argument from trap frame
    void (*entry_point)(void *) = (void (*)(void *))proc->trap_frame->sepc;
    void *arg = (void *)proc->trap_frame->a0;
    
    // Call the actual process function
    entry_point(arg);
    
    // If process returns, exit gracefully
    process_exit(0);
}

/**
 * Create a new kernel mode process
 * 
 * Allocates and initializes a process structure, allocates kernel and user
 * stacks, creates a trap frame, and adds to the scheduler ready queue.
 * 
 * @param name Process name (max PROC_NAME_LEN-1 characters)
 * @param entry_point Function to execute as process entry point
 * @param arg First argument passed to entry point
 * @return Pointer to new process, NULL on failure (panics on critical errors)
 */
struct process *process_create(const char *name, void (*entry_point)(void *), void *arg) {
    struct process *proc = alloc_process();
    if (!proc) {
        kernel_panic("process_create: Process table full");
    }
    
    // Assign unique PID
    proc->pid = alloc_pid();
    
    // Copy process name (with null terminator)
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Allocate kernel stack for trap handling and context switching
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        kernel_panic("process_create: Failed to allocate kernel stack");
    }
    
    // Use kernel page table (kernel mode process)
    proc->page_table = get_kernel_page_table();
    
    // Allocate user stack (in kernel space for kernel mode processes)
    proc->user_stack = (uintptr_t)kmalloc(USER_STACK_SIZE);
    if (!proc->user_stack) {
        kernel_panic("process_create: Failed to allocate user stack");
    }
    
    // Setup initial trap frame with entry point and argument
    setup_trap_frame(proc, entry_point, arg);
    if (!proc->trap_frame) {
        kernel_panic("process_create: Failed to allocate trap frame");
    }
    
    // Setup kernel context for initial context switch
    kmemset(&proc->context, 0, sizeof(struct context));
    // Set return address to wrapper function that calls entry_point
    proc->context.ra = (unsigned long)process_wrapper;
    // Set stack pointer to top of kernel stack (16-byte aligned for RISC-V ABI)
    proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // Initialize other process fields
    proc->cpu_time = 0;
    proc->priority = 10;  // Default priority
    proc->parent = current_process;
    proc->exit_code = 0;
    
    // Mark as ready and add to scheduler
    proc->state = PROC_READY;
    scheduler_enqueue(proc);
    
    return proc;
}

/**
 * Exit the current process
 * 
 * Marks process as zombie, removes from scheduler, and yields to another process.
 * Cannot be called on PID 0 (init process).
 * 
 * @param exit_code Exit status code (currently unused)
 */
void process_exit(int exit_code) {
    struct process *proc = current_process;
    
    if (!proc || proc->pid == 0) {
        hal_uart_puts("Cannot exit init process\n");
        while (1) {
            __asm__ volatile("wfi");
        }
    }
    
    lock_acquire(&process_lock);
    
    // Mark as zombie and record exit code
    proc->state = PROC_ZOMBIE;
    proc->exit_code = exit_code;
    
    // Remove from scheduler to prevent re-execution
    extern void scheduler_dequeue(struct process *proc);
    scheduler_dequeue(proc);
    
    lock_release(&process_lock);
    
    // Yield to another process (never returns)
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
 * User process sleep
 * 
 * Marks process as sleeping and yields to scheduler.
 * TODO: Implement timer-based wakeup queue for actual sleep functionality.
 * 
 * @param ticks Number of ticks to sleep (currently unused)
 */
void process_sleep(uint64_t ticks) {
    struct process *proc = current_process;
    if (!proc) return;
    
    lock_acquire(&process_lock);
    proc->state = PROC_SLEEPING;
    // TODO: Add to sleep queue with wakeup time
    (void)ticks;  // Mark unused parameter
    lock_release(&process_lock);
    
    process_yield();
}

/**
 * Wake up a sleeping process
 * 
 * Moves process from SLEEPING state back to READY and enqueues in scheduler.
 * 
 * @param proc Process to wake up (NULL is safe)
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

/**
 * Create a new user mode process
 * 
 * Creates a process with isolated page table and unprivileged execution context.
 * User code is mapped at USER_CODE_BASE (read-execute only).
 * User stack is allocated at USER_STACK_TOP and grows downward.
 * The process inherits kernel memory mappings for trap handling and I/O.
 * 
 * @param name Process name (for debugging, max PROC_NAME_LEN-1 chars)
 * @param user_code Pointer to user code in kernel memory to copy
 * @param code_size Size of user code in bytes
 * @return Pointer to new process, or NULL on failure
 */
struct process *process_create_user(const char *name, void *user_code, size_t code_size) {
    // Allocate process structure from process table
    struct process *proc = alloc_process();
    if (!proc) {
        return NULL;
    }
    
    // Assign unique PID
    proc->pid = alloc_pid();
    
    // Copy process name with null termination
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Create isolated user page table with kernel memory mappings
    proc->page_table = create_user_page_table();
    if (!proc->page_table) {
        process_free(proc);
        return NULL;
    }
    
    // Map user code at standard location with executable permissions
    if (map_user_code(proc->page_table, USER_CODE_BASE, user_code, code_size) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Map user stack at standard location with read-write permissions
    uintptr_t user_stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    if (map_user_memory(proc->page_table, user_stack_base, 0, USER_STACK_SIZE, 1) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Allocate kernel stack for trap handling (separate from user stack)
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        process_free(proc);
        return NULL;
    }
    
    // User stack is in user address space (already mapped above)
    proc->user_stack = user_stack_base;
    
    // Allocate trap frame to save user state on traps
    proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!proc->trap_frame) {
        process_free(proc);
        return NULL;
    }
    
    // Initialize trap frame for user mode entry
    kmemset(proc->trap_frame, 0, sizeof(struct trap_frame));
    
    // Set entry point to user code location
    proc->trap_frame->sepc = USER_CODE_BASE;
    
    // Set stack pointer to top of user stack (grows downward)
    proc->trap_frame->sp = USER_STACK_TOP;
    
    // Set sstatus for user mode return:
    // SPIE=1 (enable interrupts after sret)
    // SPP=0 (return to user mode, not supervisor)
    proc->trap_frame->sstatus = (1 << 5);  // SPIE=1, SPP=0
    
    // Setup kernel context for initial context switch
    kmemset(&proc->context, 0, sizeof(struct context));
    
    // When scheduler first runs this process, it returns to user_mode_entry_wrapper
    extern void user_mode_entry_wrapper(void);
    proc->context.ra = (unsigned long)user_mode_entry_wrapper;
    
    // Set kernel stack pointer (16-byte aligned per RISC-V ABI)
    proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // Initialize process metadata
    proc->cpu_time = 0;
    proc->priority = 10;  // Default priority (lower number = higher priority)
    proc->parent = current_process;
    proc->exit_code = 0;
    
    // Mark as ready and enqueue for scheduling
    proc->state = PROC_READY;
    scheduler_enqueue(proc);
    
    return proc;
}

/**
 * Wrapper for entering user mode from kernel context
 * 
 * Called when scheduler first switches to a user process. This function:
 * 1. Switches to the user's isolated page table
 * 2. Sets up sscratch for user-mode trap handling
 * 3. Enters user mode via sret
 * 
 * Does not return - control goes to user code.
 */
void user_mode_entry_wrapper(void) {
    struct process *proc = process_current();
    if (!proc || !proc->trap_frame || !proc->page_table) {
        kernel_panic("user_mode_entry_wrapper: invalid process state");
    }
    
    // Switch to user process page table for memory isolation
    switch_page_table(proc->page_table);
    
    // Setup sscratch with kernel stack pointer for trap entry
    // When trap occurs in user mode, sscratch will swap with sp
    uintptr_t kernel_sp = proc->kernel_stack + KERNEL_STACK_SIZE;
    __asm__ volatile("csrw sscratch, %0" :: "r"(kernel_sp));
    
    // Enter user mode - never returns (continues in user code or via trap)
    user_return(proc->trap_frame);
    
    // Should never reach here
    kernel_panic("user_mode_entry_wrapper: user_return returned");
}
