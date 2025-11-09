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
        free_page_table(proc->page_table);
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
        kernel_panic("process_create: Process table full - cannot allocate process");
    }
    
    // Assign PID
    proc->pid = alloc_pid();
    
    // Copy name
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Allocate kernel stack
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        kernel_panic("process_create: Failed to allocate kernel stack");
    }
    
    // For now, create kernel-mode processes only (use kernel page table)
    // TODO: Create user-mode processes with separate page tables
    proc->page_table = get_kernel_page_table();
    
    // Allocate user stack (in kernel space for now)
    proc->user_stack = (uintptr_t)kmalloc(USER_STACK_SIZE);
    if (!proc->user_stack) {
        kernel_panic("process_create: Failed to allocate user stack");
    }
    
    // Setup initial trap frame
    setup_trap_frame(proc, entry_point, arg);
    if (!proc->trap_frame) {
        kernel_panic("process_create: Failed to allocate trap frame");
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

/**
 * Create a new user-mode process
 * 
 * Creates a process with its own page table and user address space.
 * The user code is mapped at USER_CODE_BASE with executable permissions.
 * The user stack is allocated at USER_STACK_TOP.
 * 
 * @param name Process name
 * @param user_code Pointer to user code to execute (in kernel memory)
 * @param code_size Size of user code in bytes
 * @return Pointer to new process, or NULL on failure
 */
struct process *process_create_user(const char *name, void *user_code, size_t code_size) {
    hal_uart_puts("Creating user process: ");
    hal_uart_puts(name);
    hal_uart_puts("\n");
    
    // Allocate process structure
    struct process *proc = alloc_process();
    if (!proc) {
        hal_uart_puts("process_create_user: Failed to allocate process\n");
        return NULL;
    }
    
    // Assign PID
    proc->pid = alloc_pid();
    
    // Copy name
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Create user page table (with kernel mappings)
    proc->page_table = create_user_page_table();
    if (!proc->page_table) {
        hal_uart_puts("process_create_user: Failed to create page table\n");
        process_free(proc);
        return NULL;
    }
    
    // Map user code at USER_CODE_BASE
    hal_uart_puts("Mapping user code (");
    kprint_dec(code_size);
    hal_uart_puts(" bytes) at 0x");
    kprint_hex(USER_CODE_BASE);
    hal_uart_puts("\n");
    
    if (map_user_code(proc->page_table, USER_CODE_BASE, user_code, code_size) != 0) {
        hal_uart_puts("process_create_user: Failed to map user code\n");
        process_free(proc);
        return NULL;
    }
    
    // Allocate and map user stack
    hal_uart_puts("Mapping user stack at 0x");
    kprint_hex(USER_STACK_TOP - USER_STACK_SIZE);
    hal_uart_puts("\n");
    
    uintptr_t user_stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    if (map_user_memory(proc->page_table, user_stack_base, 0, USER_STACK_SIZE, 1) != 0) {
        hal_uart_puts("process_create_user: Failed to map user stack\n");
        process_free(proc);
        return NULL;
    }
    
    // Allocate kernel stack
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        hal_uart_puts("process_create_user: Failed to allocate kernel stack\n");
        process_free(proc);
        return NULL;
    }
    
    // User stack is in user space (not allocated here)
    proc->user_stack = user_stack_base;
    
    // Allocate trap frame
    proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!proc->trap_frame) {
        hal_uart_puts("process_create_user: Failed to allocate trap frame\n");
        process_free(proc);
        return NULL;
    }
    
    // Setup trap frame for user mode entry
    kmemset(proc->trap_frame, 0, sizeof(struct trap_frame));
    
    // Set PC to user code entry point
    proc->trap_frame->sepc = USER_CODE_BASE;
    
    // Set user stack pointer (top of stack, grows downward)
    proc->trap_frame->sp = USER_STACK_TOP;
    
    // Clear all registers
    proc->trap_frame->ra = 0;
    proc->trap_frame->gp = 0;
    proc->trap_frame->tp = 0;
    proc->trap_frame->a0 = 0;
    proc->trap_frame->a1 = 0;
    proc->trap_frame->a2 = 0;
    proc->trap_frame->a3 = 0;
    proc->trap_frame->a4 = 0;
    proc->trap_frame->a5 = 0;
    proc->trap_frame->a6 = 0;
    proc->trap_frame->a7 = 0;
    
    // Set sstatus for user mode:
    // - SPP=0 (return to user mode)
    // - SPIE=1 (enable interrupts after sret)
    // - SIE=0 (interrupts disabled in supervisor mode, but we're going to user mode)
    proc->trap_frame->sstatus = (1 << 5);  // SPIE=1, SPP=0
    
    // Setup kernel context for initial switch
    kmemset(&proc->context, 0, sizeof(struct context));
    
    // When scheduler switches to this process for the first time,
    // it will "return" to user_mode_entry_wrapper
    extern void user_mode_entry_wrapper(void);
    proc->context.ra = (unsigned long)user_mode_entry_wrapper;
    
    // Set kernel stack pointer
    proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // Initialize other fields
    proc->cpu_time = 0;
    proc->priority = 10;
    proc->parent = current_process;
    proc->exit_code = 0;
    
    // Mark as ready
    proc->state = PROC_READY;
    
    // Add to scheduler
    scheduler_enqueue(proc);
    
    hal_uart_puts("User process created with PID ");
    kprint_dec(proc->pid);
    hal_uart_puts("\n");
    
    return proc;
}

/**
 * Wrapper for user mode entry
 * 
 * When scheduler switches to a user process for the first time,
 * this function is called. It switches to the user page table
 * and enters user mode via user_return().
 */
void user_mode_entry_wrapper(void) {
    struct process *proc = process_current();
    if (!proc || !proc->trap_frame || !proc->page_table) {
        hal_uart_puts("Error: Invalid process state in user_mode_entry_wrapper\n");
        kernel_panic("user_mode_entry_wrapper: invalid process");
    }
    
    hal_uart_puts("Switching to user process PID ");
    kprint_dec(proc->pid);
    hal_uart_puts("\n");
    
    // Switch to user page table
    hal_uart_puts("Switching to user page table\n");
    switch_page_table(proc->page_table);
    
    // Setup sscratch with kernel stack pointer for trap handling
    // When we enter user mode, traps will swap sp with sscratch
    uintptr_t kernel_sp = proc->kernel_stack + KERNEL_STACK_SIZE;
    __asm__ volatile("csrw sscratch, %0" :: "r"(kernel_sp));
    
    hal_uart_puts("Entering user mode at PC=0x");
    kprint_hex(proc->trap_frame->sepc);
    hal_uart_puts(" SP=0x");
    kprint_hex(proc->trap_frame->sp);
    hal_uart_puts("\n");
    
    // Enter user mode (never returns)
    user_return(proc->trap_frame);
    
    // Should never reach here
    kernel_panic("user_mode_entry_wrapper: user_return returned");
}
