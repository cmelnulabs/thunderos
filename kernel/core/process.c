/*
 * Process Management Implementation
 */

#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/kstring.h"
#include "kernel/panic.h"
#include "kernel/signal.h"
#include "kernel/errno.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "mm/paging.h"
#include "hal/hal_uart.h"
#include "kernel/elf_loader.h"
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

// Forward declarations
static void forked_child_entry(void);

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
    init_proc->errno_value = 0;
    init_proc->trap_frame = NULL;
    init_proc->cwd[0] = '/';
    init_proc->cwd[1] = '\0';
    init_proc->controlling_tty = 0;  /* Kernel console (VT1) */
    
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
    
    // Clean up VMAs first
    process_cleanup_vmas(proc);
    
    // Free kernel stack (this WAS allocated with kmalloc)
    if (proc->kernel_stack) {
        kfree((void *)proc->kernel_stack);
    }
    
    // NOTE: Do NOT kfree user_stack! It's a USER SPACE virtual address,
    // not a kernel allocation. The user stack pages are freed when we
    // free the page table (which unmaps and frees all user pages).
    // if (proc->user_stack) {
    //     kfree((void *)proc->user_stack);  // WRONG!
    // }
    
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
    proc->errno_value = 0;
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';
    proc->controlling_tty = current_process ? current_process->controlling_tty : 0;
    
    // Initialize signals
    extern void signal_init_process(struct process *proc);
    signal_init_process(proc);
    
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
    
    // Save parent pointer before acquiring lock
    struct process *parent = proc->parent;
    
    lock_acquire(&process_lock);
    
    // Mark as zombie and record exit code
    proc->state = PROC_ZOMBIE;
    proc->exit_code = exit_code;
    
    // Remove from scheduler to prevent re-execution
    extern void scheduler_dequeue(struct process *proc);
    scheduler_dequeue(proc);
    
    lock_release(&process_lock);
    
    // Send SIGCHLD to parent AFTER releasing lock to avoid deadlock
    // (signal_send -> process_wakeup also acquires process_lock)
    if (parent) {
        extern int signal_send(struct process *target, int signum);
        signal_send(parent, SIGCHLD);
    }
    
    // Keep yielding until scheduler finds another process
    // This process is now a zombie and should never run again
    while (1) {
        process_yield();
        // If we get here, there's no other process to run
        // Just wait for interrupts (shouldn't happen in practice)
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
 * Find a zombie child process
 * 
 * @param parent Parent process
 * @param target_pid PID to search for (-1 for any child)
 * @return Zombie child process, or NULL if none found
 */
struct process *process_find_zombie_child(struct process *parent, int target_pid) {
    if (!parent) return NULL;
    
    lock_acquire(&process_lock);
    
    // Search through process table for zombie children
    for (int i = 0; i < MAX_PROCS; i++) {
        struct process *proc = &process_table[i];
        
        if (proc->state == PROC_ZOMBIE && proc->parent == parent) {
            // Found a zombie child
            if (target_pid == -1 || proc->pid == target_pid) {
                lock_release(&process_lock);
                return proc;
            }
        }
    }
    
    lock_release(&process_lock);
    return NULL;
}

/**
 * Check if parent has any children matching criteria
 * 
 * @param parent Parent process
 * @param target_pid PID to search for (-1 for any child)
 * @return 1 if children exist, 0 otherwise
 */
int process_has_children(struct process *parent, int target_pid) {
    if (!parent) return 0;
    
    lock_acquire(&process_lock);
    
    // Search through process table for children
    for (int i = 0; i < MAX_PROCS; i++) {
        struct process *proc = &process_table[i];
        
        if (proc->state != PROC_UNUSED && proc->parent == parent) {
            // Found a child
            if (target_pid == -1 || proc->pid == target_pid) {
                lock_release(&process_lock);
                return 1;
            }
        }
    }
    
    lock_release(&process_lock);
    return 0;
}

/**
 * Get process by index in process table
 * 
 * Used for iterating through all processes (e.g., for ps command).
 * 
 * @param index Index in process table (0 to MAX_PROCS-1)
 * @return Process pointer, or NULL if index invalid or slot unused
 */
struct process *process_get_by_index(int index) {
    if (index < 0 || index >= MAX_PROCS) {
        return NULL;
    }
    
    struct process *p = &process_table[index];
    if (p->state == PROC_UNUSED) {
        return NULL;
    }
    
    return p;
}

/**
 * Get maximum number of processes
 * 
 * @return Maximum process count (MAX_PROCS)
 */
int process_get_max_count(void) {
    return MAX_PROCS;
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
 * 
 * Creates a copy of the current process with complete memory isolation.
 * Copies page table, VMAs, and process state.
 * 
 * @param current_tf Current trap frame with register state to copy to child
 * @return Child PID in parent, 0 in child, -1 on error
 */
pid_t process_fork(struct trap_frame *current_tf) {
    struct process *parent = process_current();
    if (!parent) {
        hal_uart_puts("process_fork: no current process\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    if (!current_tf) {
        hal_uart_puts("process_fork: NULL trap frame\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Allocate new process structure
    struct process *child = alloc_process();
    if (!child) {
        hal_uart_puts("process_fork: process table full\n");
        RETURN_ERRNO(THUNDEROS_EAGAIN);
    }
    
    // Assign new PID
    child->pid = alloc_pid();
    
    // Copy basic process info
    kstrcpy(child->name, parent->name);
    child->state = PROC_READY;
    child->parent = parent;
    child->cpu_time = 0;
    child->priority = parent->priority;
    child->exit_code = 0;
    child->errno_value = 0;
    child->controlling_tty = parent->controlling_tty;  /* Inherit parent's TTY */
    
    /* Copy parent's current working directory with proper null termination */
    int cwd_index = 0;
    for (cwd_index = 0; cwd_index < 255 && parent->cwd[cwd_index]; cwd_index++) {
        child->cwd[cwd_index] = parent->cwd[cwd_index];
    }
    child->cwd[cwd_index] = '\0';
    
    /* Allocate kernel stack for child */
    child->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!child->kernel_stack) {
        hal_uart_puts("process_fork: failed to allocate kernel stack\n");
        process_free(child);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    // Set up memory isolation for child
    if (process_setup_memory_isolation(child) != 0) {
        hal_uart_puts("process_fork: failed to setup memory isolation\n");
        process_free(child);
        /* errno already set by process_setup_memory_isolation */
        return -1;
    }
    
    // Create new page table for child
    child->page_table = create_user_page_table();
    if (!child->page_table) {
        hal_uart_puts("process_fork: failed to create page table\n");
        process_free(child);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    // Copy VMAs from parent to child
    vm_area_t *parent_vma = parent->vm_areas;
    while (parent_vma) {
        // Add VMA to child
        if (process_add_vma(child, parent_vma->start, parent_vma->end, parent_vma->flags) != 0) {
            hal_uart_puts("process_fork: failed to copy VMA\n");
            process_free(child);
            /* errno already set by process_add_vma */
            return -1;
        }
        
        // Copy physical pages for this VMA
        for (uint64_t addr = parent_vma->start; addr < parent_vma->end; addr += PAGE_SIZE) {
            uintptr_t parent_paddr;
            if (virt_to_phys(parent->page_table, addr, &parent_paddr) == 0) {
                // Allocate new physical page for child
                uintptr_t child_paddr = pmm_alloc_page();
                if (!child_paddr) {
                    hal_uart_puts("process_fork: failed to allocate page\n");
                    process_free(child);
                    RETURN_ERRNO(THUNDEROS_ENOMEM);
                }
                
                // Copy page contents using physical addresses (identity-mapped in kernel)
                kmemcpy((void *)child_paddr, (void *)parent_paddr, PAGE_SIZE);
                
                // Convert VM flags to PTE flags
                uint64_t pte_flags = PTE_V;
                if (parent_vma->flags & VM_READ) pte_flags |= PTE_R;
                if (parent_vma->flags & VM_WRITE) pte_flags |= PTE_W;
                if (parent_vma->flags & VM_EXEC) pte_flags |= PTE_X;
                if (parent_vma->flags & VM_USER) pte_flags |= PTE_U;
                
                // Map page in child's page table
                if (map_page(child->page_table, addr, child_paddr, pte_flags) != 0) {
                    hal_uart_puts("process_fork: failed to map page\n");
                    pmm_free_page(child_paddr);
                    process_free(child);
                    /* errno already set by map_page */
                    return -1;
                }
            }
        }
        
        parent_vma = parent_vma->next;
    }
    
    // Copy heap information
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->user_stack = parent->user_stack;
    
    // Allocate and copy trap frame
    child->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!child->trap_frame) {
        hal_uart_puts("process_fork: failed to allocate trap frame\n");
        process_free(child);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    // Copy from CURRENT trap frame (on kernel stack), not old parent->trap_frame
    kmemcpy(child->trap_frame, current_tf, sizeof(struct trap_frame));
    
    // NOTE: We do NOT update parent->trap_frame here! The parent is still executing
    // in the syscall, and parent->trap_frame will be updated by the trap handler
    // when it returns (or during next context switch if preempted).
    
    // Set return value to 0 in child (distinguish from parent)
    child->trap_frame->a0 = 0;
    
    // CRITICAL: Advance sepc past the ecall instruction!
    // The parent's trap frame still has sepc pointing AT the ecall (not past it)
    // because the trap handler hasn't returned yet. We need to advance it so
    // the child returns to the instruction AFTER the ecall.
    child->trap_frame->sepc += 4;
    
    // Initialize child's kernel context for first schedule
    kmemset(&child->context, 0, sizeof(struct context));
    
    // Set up child's kernel stack pointer
    child->context.sp = child->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // CRITICAL: Initialize s0 (frame pointer) to point to kernel stack!
    // forked_child_entry is a C function and with -O0 it uses frame pointers.
    // It needs s0 to point to a valid KERNEL stack location, not user space.
    // Set s0 = sp (both point to top of kernel stack).
    child->context.s0 = child->context.sp;
    
    // Other callee-saved registers (s1-s11) can stay at 0 - they're not used
    // by forked_child_entry before user_return restores them from trap_frame.
    
    // Set up child to return to user mode on first schedule
    // Use forked_child_entry which restores all registers from trap frame
    child->context.ra = (unsigned long)forked_child_entry;
    
    // Add child to scheduler
    child->state = PROC_READY;
    scheduler_enqueue(child);
    
    // Return child PID to parent
    return child->pid;
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
    
    // Set up memory isolation (VMA tracking and heap)
    if (process_setup_memory_isolation(proc) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Map user code at standard location with executable permissions
    if (map_user_code(proc->page_table, USER_CODE_BASE, user_code, code_size) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Add VMA for code segment
    uint64_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t code_end = USER_CODE_BASE + (code_pages * PAGE_SIZE);
    if (process_add_vma(proc, USER_CODE_BASE, code_end, VM_READ | VM_EXEC | VM_USER) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Map user stack at standard location with read-write permissions
    uintptr_t user_stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    if (map_user_memory(proc->page_table, user_stack_base, 0, USER_STACK_SIZE, 1) != 0) {
        process_free(proc);
        return NULL;
    }
    
    // Add VMA for stack segment
    if (process_add_vma(proc, user_stack_base, USER_STACK_TOP, VM_READ | VM_WRITE | VM_USER | VM_GROWSDOWN) != 0) {
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
    // SIE=0 (disable interrupts in supervisor mode during transition)
    // SUM=1 (allow supervisor to access user memory during transition)
    // We need to preserve other bits from current sstatus, only modify SPP, SPIE, SIE, SUM
    unsigned long sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(1 << 8);   // Clear SPP (bit 8) = return to user mode
    sstatus &= ~(1 << 1);   // Clear SIE (bit 1) = disable interrupts during user_return
    sstatus |= (1 << 5);    // Set SPIE (bit 5) = enable interrupts after sret
    sstatus |= (1UL << 18); // Set SUM (bit 18) = allow supervisor access to user memory
    proc->trap_frame->sstatus = sstatus;
    
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
    proc->errno_value = 0;
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';
    proc->controlling_tty = current_process ? current_process->controlling_tty : 0;
    
    // Mark as ready and enqueue for scheduling
    proc->state = PROC_READY;
    scheduler_enqueue(proc);
    
    return proc;
}

/**
 * Create a new user process from loaded ELF segments
 * 
 * Unlike process_create_user which maps code at a fixed address (USER_CODE_BASE),
 * this function maps code at the virtual address specified by the ELF file,
 * and sets the entry point to the ELF entry address.
 */
struct process *process_create_elf(const char *name, uint64_t code_base, 
                                   void *code_mem, size_t code_size, 
                                   uint64_t entry_point) {
    if (!name || !code_mem || code_size == 0) {
        return NULL;
    }
    
    // Allocate process structure
    struct process *proc = alloc_process();
    if (!proc) {
        return NULL;
    }
    
    // Assign unique PID
    proc->pid = alloc_pid();
    
    // Set process name
    kstrncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    // Allocate kernel stack
    proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        process_free(proc);
        return NULL;
    }
    
    // Create isolated page table for this process
    proc->page_table = create_user_page_table();
    if (!proc->page_table) {
        kfree((void *)proc->kernel_stack);
        process_free(proc);
        return NULL;
    }
    
    // Map user code at the specified virtual address base
    // Round size up to page boundary
    size_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < code_pages; i++) {
        uintptr_t vaddr = code_base + (i * PAGE_SIZE);
        uintptr_t paddr = (uintptr_t)code_mem + (i * PAGE_SIZE);
        
        // Map as user-readable, writable, and executable
        // TODO: Use proper segment permissions from ELF (R/W/X per segment)
        if (map_page(proc->page_table, vaddr, paddr, 
                           PTE_V | PTE_R | PTE_W | PTE_X | PTE_U) != 0) {
            free_page_table(proc->page_table);
            kfree((void *)proc->kernel_stack);
            process_free(proc);
            return NULL;
        }
    }
    
    // Allocate user stack (multiple pages for stack growth)
    #define INITIAL_STACK_PAGES 8  // 32KB initial stack
    uintptr_t stack_base_vaddr = USER_STACK_TOP - (INITIAL_STACK_PAGES * PAGE_SIZE);
    
    for (int i = 0; i < INITIAL_STACK_PAGES; i++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            free_page_table(proc->page_table);
            kfree((void *)proc->kernel_stack);
            process_free(proc);
            return NULL;
        }
        
        // Zero out stack via virtual address
        kmemset((void *)translate_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        
        // Map stack page
        uintptr_t stack_vaddr = stack_base_vaddr + (i * PAGE_SIZE);
        if (map_page(proc->page_table, stack_vaddr, stack_phys,
                           PTE_V | PTE_R | PTE_W | PTE_U) != 0) {
            free_page_table(proc->page_table);
            kfree((void *)proc->kernel_stack);
            process_free(proc);
            return NULL;
        }
    }
    
    proc->user_stack = stack_base_vaddr;
    
    // Allocate trap frame to save user state on traps
    proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));
    if (!proc->trap_frame) {
        free_page_table(proc->page_table);
        kfree((void *)proc->kernel_stack);
        process_free(proc);
        return NULL;
    }
    
        // Zero trap frame
    kmemset(proc->trap_frame, 0, sizeof(struct trap_frame));
    
    // Set entry point to the ELF entry address
    proc->trap_frame->sepc = entry_point;
    
    // Set stack pointer to top of user stack (grows downward)
    proc->trap_frame->sp = USER_STACK_TOP;
    
    // CRITICAL: Initialize frame pointer (s0/fp) for -O0 compiled code
    // With -O0, GCC uses s0 as frame pointer, so it must point to valid stack
    proc->trap_frame->s0 = USER_STACK_TOP;
    
    // Set sstatus for user mode return:
    // SPIE=1 (enable interrupts after sret)
    // SPP=0 (return to user mode, not supervisor)
    // SIE=0 (disable interrupts in supervisor mode during transition)
    // SUM=1 (allow supervisor to access user memory during transition)
    // We need to preserve other bits from current sstatus, only modify SPP, SPIE, SIE, SUM
    unsigned long sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(1 << 8);   // Clear SPP (bit 8) = return to user mode
    sstatus &= ~(1 << 1);   // Clear SIE (bit 1) = disable interrupts during user_return
    sstatus |= (1 << 5);    // Set SPIE (bit 5) = enable interrupts after sret
    sstatus |= (1UL << 18); // Set SUM (bit 18) = allow supervisor access to user memory
    proc->trap_frame->sstatus = sstatus;
    
    // Setup kernel context for initial context switch
    kmemset(&proc->context, 0, sizeof(struct context));
    
    // When scheduler first runs this process, it returns to user_mode_entry_wrapper
    extern void user_mode_entry_wrapper(void);
    proc->context.ra = (unsigned long)user_mode_entry_wrapper;
    
    // Set kernel stack pointer (16-byte aligned per RISC-V ABI)
    proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;
    
    // Initialize process metadata
    proc->cpu_time = 0;
    proc->priority = 10;  // Default priority
    proc->parent = current_process;
    proc->exit_code = 0;
    proc->errno_value = 0;
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';
    proc->controlling_tty = current_process ? current_process->controlling_tty : 0;
    
    // Setup memory isolation (VMAs for validation)
    if (process_setup_memory_isolation(proc) != 0) {
        free_page_table(proc->page_table);
        kfree((void *)proc->kernel_stack);
        kfree(proc->trap_frame);
        process_free(proc);
        /* errno already set by process_setup_memory_isolation */
        return NULL;
    }
    
    // Add VMAs for code segment
    if (process_add_vma(proc, code_base, code_base + code_size, 
                       VM_READ | VM_WRITE | VM_EXEC | VM_USER) != 0) {
        process_cleanup_vmas(proc);
        free_page_table(proc->page_table);
        kfree((void *)proc->kernel_stack);
        kfree(proc->trap_frame);
        process_free(proc);
        return NULL;
    }
    
    // Add VMA for stack segment
    if (process_add_vma(proc, stack_base_vaddr, USER_STACK_TOP,
                       VM_READ | VM_WRITE | VM_USER | VM_GROWSDOWN) != 0) {
        process_cleanup_vmas(proc);
        free_page_table(proc->page_table);
        kfree((void *)proc->kernel_stack);
        kfree(proc->trap_frame);
        process_free(proc);
        return NULL;
    }
    
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

/**
 * Wrapper for forked child to enter user mode
 * 
 * Different from user_mode_entry_wrapper because it needs to restore
 * ALL registers from trap frame (the parent's state at fork time).
 */
void forked_child_entry(void) {
    struct process *proc = process_current();
    
    if (!proc || !proc->trap_frame || !proc->page_table) {
        kernel_panic("forked_child_entry: invalid process state");
    }
    
    // CRITICAL: Switch to child's page table BEFORE entering user mode!
    // The child has its own copy of user memory (from fork). If we use the
    // parent's page table, the child would read/write parent's memory instead
    // of its own copy. The trap_frame is in kernel memory (kmalloc) so it's
    // accessible regardless of which user page table is active.
    switch_page_table(proc->page_table);
    
    // Setup sscratch with kernel stack pointer for trap entry
    uintptr_t kernel_sp = proc->kernel_stack + KERNEL_STACK_SIZE;
    __asm__ volatile("csrw sscratch, %0" :: "r"(kernel_sp));
    
    // Return to user mode using user_return which restores all registers
    extern void user_return(struct trap_frame *tf);
    user_return(proc->trap_frame);
    
    kernel_panic("forked_child_entry: user_return returned");
}

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
    
    // Enter user mode using specialized entry function
    // This avoids the problem of loading sp before sret in user_return()
    extern void enter_user_mode_asm(unsigned long user_sp, unsigned long entry, unsigned long sstatus_val);
    enter_user_mode_asm(proc->trap_frame->sp, proc->trap_frame->sepc, proc->trap_frame->sstatus);
    
    // Should never reach here
    kernel_panic("Returned from enter_user_mode_asm!");
    
    // Should never reach here
    kernel_panic("user_mode_entry_wrapper: user_return returned");
}

/**
 * Set up memory isolation for a process
 * 
 * Initializes VMA list and heap boundaries for complete memory isolation.
 * Called during process creation.
 * 
 * @param proc Process to setup
 * @return 0 on success, -1 on failure
 */
int process_setup_memory_isolation(struct process *proc) {
    if (!proc) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Initialize VMA list as empty
    proc->vm_areas = NULL;
    
    // Initialize heap boundaries (heap starts above code and data)
    proc->heap_start = USER_HEAP_BASE;
    proc->heap_end = USER_HEAP_BASE;
    
    return 0;
}

/**
 * Find VMA containing an address
 * 
 * Searches the process's VMA list for a region containing the given address.
 * 
 * @param proc Process to search
 * @param addr Virtual address to find
 * @return Pointer to VMA if found, NULL otherwise
 */
vm_area_t *process_find_vma(struct process *proc, uint64_t addr) {
    if (!proc) {
        return NULL;
    }
    
    vm_area_t *vma = proc->vm_areas;
    while (vma) {
        if (addr >= vma->start && addr < vma->end) {
            return vma;
        }
        vma = vma->next;
    }
    
    return NULL;
}

/**
 * Add a VMA to process address space
 * 
 * Creates a new VMA and adds it to the process's VMA list.
 * 
 * @param proc Process to add VMA to
 * @param start Start address (inclusive)
 * @param end End address (exclusive)
 * @param flags Protection flags (VM_READ, VM_WRITE, VM_EXEC, VM_USER)
 * @return 0 on success, -1 on failure
 */
int process_add_vma(struct process *proc, uint64_t start, uint64_t end, uint32_t flags) {
    if (!proc || start >= end) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Allocate new VMA structure
    vm_area_t *vma = (vm_area_t *)kmalloc(sizeof(vm_area_t));
    if (!vma) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    // Initialize VMA
    vma->start = start;
    vma->end = end;
    vma->flags = flags;
    
    // Add to front of list
    vma->next = proc->vm_areas;
    proc->vm_areas = vma;
    
    return 0;
}

/**
 * Remove a VMA from process address space
 * 
 * Removes and frees a VMA from the process's VMA list.
 * 
 * @param proc Process to remove VMA from
 * @param vma VMA to remove
 */
void process_remove_vma(struct process *proc, vm_area_t *vma) {
    if (!proc || !vma) {
        return;
    }
    
    // Find and unlink from list
    vm_area_t **prev = &proc->vm_areas;
    while (*prev) {
        if (*prev == vma) {
            *prev = vma->next;
            kfree(vma);
            return;
        }
        prev = &(*prev)->next;
    }
}

/**
 * Clean up all VMAs for a process
 * 
 * Frees all VMA structures in the process's VMA list.
 * Called during process cleanup.
 * 
 * @param proc Process to cleanup
 */
void process_cleanup_vmas(struct process *proc) {
    if (!proc) {
        return;
    }
    
    vm_area_t *vma = proc->vm_areas;
    while (vma) {
        vm_area_t *next = vma->next;
        kfree(vma);
        vma = next;
    }
    
    proc->vm_areas = NULL;
}

/**
 * Map a memory region in process address space
 * 
 * Allocates physical pages and maps them into the process's virtual address space.
 * Also creates a VMA to track the mapping.
 * 
 * @param proc Process to map in
 * @param vaddr Virtual address to start mapping (must be page-aligned)
 * @param size Size in bytes
 * @param flags Protection flags (VM_READ, VM_WRITE, VM_EXEC, VM_USER)
 * @return 0 on success, -1 on failure
 */
int process_map_region(struct process *proc, uint64_t vaddr, uint64_t size, uint32_t flags) {
    if (!proc || !proc->page_table || size == 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Align to page boundaries
    uint64_t start = vaddr & ~(PAGE_SIZE - 1);
    uint64_t end = (vaddr + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Convert VM flags to PTE flags
    uint64_t pte_flags = PTE_V;
    if (flags & VM_READ) pte_flags |= PTE_R;
    if (flags & VM_WRITE) pte_flags |= PTE_W;
    if (flags & VM_EXEC) pte_flags |= PTE_X;
    if (flags & VM_USER) pte_flags |= PTE_U;
    
    // Allocate and map each page
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uintptr_t phys_page = pmm_alloc_page();
        if (!phys_page) {
            // TODO: Cleanup already mapped pages
            RETURN_ERRNO(THUNDEROS_ENOMEM);
        }
        
        // Zero the page
        kmemset((void *)phys_page, 0, PAGE_SIZE);
        
        // Map the page
        if (map_page(proc->page_table, addr, phys_page, pte_flags) != 0) {
            pmm_free_page(phys_page);
            // TODO: Cleanup already mapped pages
            /* errno already set by map_page */
            return -1;
        }
    }
    
    // Add VMA to track this region
    if (process_add_vma(proc, start, end, flags) != 0) {
        // TODO: Cleanup mapped pages
        /* errno already set by process_add_vma */
        return -1;
    }
    
    return 0;
}

/**
 * Validate user pointer against process VMAs
 * 
 * Checks if a user-space pointer is valid for the specified access type.
 * Ensures the pointer falls within a mapped VMA with appropriate permissions.
 * 
 * @param proc Process to validate for
 * @param ptr Pointer to validate
 * @param size Size of memory region
 * @param required_flags Flags that must be set (e.g., VM_WRITE for write access)
 * @return 1 if valid, 0 if invalid
 */
int process_validate_user_ptr(struct process *proc, const void *ptr, size_t size, uint32_t required_flags) {
    if (!proc || !ptr || size == 0) {
        return 0;
    }
    
    uint64_t start = (uint64_t)ptr;
    uint64_t end = start + size;
    
    // Check for kernel space addresses
    if (start >= KERNEL_VIRT_BASE || end >= KERNEL_VIRT_BASE) {
        return 0;  // Trying to access kernel memory
    }
    
    // Check for overflow
    if (end < start) {
        return 0;
    }
    
    // Find VMA containing start address
    vm_area_t *vma = process_find_vma(proc, start);
    if (!vma) {
        return 0;  // Start address not mapped
    }
    
    // Check if entire range is within VMA
    if (end > vma->end) {
        return 0;  // Range extends beyond VMA
    }
    
    // Check permissions
    if ((vma->flags & required_flags) != required_flags) {
        return 0;  // Missing required permissions
    }
    
    return 1;
}

/**
 * Set the controlling terminal for a process
 * 
 * @param proc Process to set terminal for
 * @param tty_index Terminal index (0 to 5), or -1 for none
 * @return 0 on success, -1 on error
 */
int process_set_tty(struct process *proc, int tty_index) {
    if (!proc) {
        return -1;
    }
    
    /* Allow -1 (no controlling terminal) or 0-5 (valid VT index) */
    if (tty_index < -1 || tty_index >= 6) {
        return -1;
    }
    
    proc->controlling_tty = tty_index;
    return 0;
}

/**
 * Get the controlling terminal for a process
 * 
 * @param proc Process to get terminal for
 * @return Terminal index, or -1 if none or error
 */
int process_get_tty(struct process *proc) {
    if (!proc) {
        return -1;
    }
    
    return proc->controlling_tty;
}
