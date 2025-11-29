/*
 * Process Management for ThunderOS
 * 
 * This module provides process creation, scheduling, and context switching
 * for RISC-V architecture.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "trap.h"
#include "mm/paging.h"

// Forward declaration
typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);

#define NSIG 32

// Process states
typedef enum {
    PROC_UNUSED = 0,    // Process slot is unused
    PROC_EMBRYO,        // Process being created
    PROC_READY,         // Ready to run
    PROC_RUNNING,       // Currently running
    PROC_SLEEPING,      // Waiting for event
    PROC_ZOMBIE         // Exited but not yet cleaned up
} proc_state_t;

// Process ID type
typedef int32_t pid_t;

// Maximum number of processes
#define MAX_PROCS 64

// Process name length
#define PROC_NAME_LEN 32

// User stack size (1MB)
#define USER_STACK_SIZE (1024 * 1024)

// Kernel stack size (16KB)
#define KERNEL_STACK_SIZE (16 * 1024)

// RISC-V ABI requires 16-byte stack alignment
#define STACK_ALIGNMENT 16

// User space memory layout
#define USER_CODE_BASE    0x0000000000010000  // User code starts at 64KB
#define USER_STACK_TOP    0x0000000040000000  // User stack top at 1GB (in user space)
#define USER_HEAP_BASE    0x0000000000100000  // User heap base (future)
#define USER_MMAP_START   0x40000000     // Memory mapped region (1GB)

// Heap safety margin to prevent collision with stack
#define HEAP_STACK_SAFETY_MARGIN (1024 * 1024)  // 1MB

// Memory protection flags for VMAs
#define VM_READ     0x01  // Readable
#define VM_WRITE    0x02  // Writable
#define VM_EXEC     0x04  // Executable
#define VM_USER     0x08  // User accessible
#define VM_SHARED   0x10  // Shared mapping
#define VM_GROWSDOWN 0x20 // Stack segment (grows downward)

// Virtual memory area - tracks mapped memory regions
typedef struct vm_area {
    uint64_t start;           // Start virtual address (inclusive)
    uint64_t end;             // End virtual address (exclusive)
    uint32_t flags;           // Protection flags (VM_READ, VM_WRITE, etc.)
    struct vm_area *next;     // Next VMA in list
} vm_area_t;

// Process context - saved during context switch
struct context {
    unsigned long ra;   // Return address
    unsigned long sp;   // Stack pointer
    unsigned long s0;   // Saved registers s0-s11
    unsigned long s1;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
};

// Process control block (PCB)
struct process {
    pid_t pid;                          // Process ID
    proc_state_t state;                 // Process state
    char name[PROC_NAME_LEN];           // Process name
    
    // Memory management - ISOLATION CRITICAL
    page_table_t *page_table;           // Virtual memory page table (isolated per-process)
    uintptr_t kernel_stack;             // Kernel stack base
    uintptr_t user_stack;               // User stack base (virtual)
    vm_area_t *vm_areas;                // List of mapped virtual memory areas
    uint64_t heap_start;                // Heap start address
    uint64_t heap_end;                  // Current heap end (brk)
    
    // Saved context (for context switching)
    struct context context;             // Kernel context
    struct trap_frame *trap_frame;      // User context (trap frame)
    
    // Scheduling
    uint64_t cpu_time;                  // Total CPU time used (in ticks)
    uint64_t priority;                  // Scheduling priority (lower = higher priority)
    
    // Process tree
    struct process *parent;             // Parent process
    
    // Exit status
    int exit_code;                      // Exit code if state is ZOMBIE
    
    // Error handling
    int errno_value;                    // Per-process error number (errno)
    
    // Signal handling
    sigset_t pending_signals;           // Pending signals (bitmask)
    sigset_t blocked_signals;           // Blocked signals (bitmask)
    sighandler_t signal_handlers[NSIG]; // Signal handler functions
    
    // Current working directory
    char cwd[256];                      // Current working directory path
    
    // Console multiplexing
    int controlling_tty;                // Controlling terminal index (-1 = none)
};

/**
 * Initialize the process management subsystem
 * 
 * Sets up process table and creates the initial kernel process.
 */
void process_init(void);

/**
 * Create a new process
 * 
 * @param name Process name
 * @param entry_point Entry point function
 * @param arg Argument to pass to entry point
 * @return Pointer to new process, or NULL on failure
 */
struct process *process_create(const char *name, void (*entry_point)(void *), void *arg);

/**
 * Exit the current process
 * 
 * @param exit_code Exit status code
 */
void process_exit(int exit_code) __attribute__((noreturn));

/**
 * Get the currently running process
 * 
 * @return Pointer to current process
 */
struct process *process_current(void);

/**
 * Get process by PID
 * 
 * @param pid Process ID
 * @return Pointer to process, or NULL if not found
 */
struct process *process_get(pid_t pid);

/**
 * Yield CPU to another process
 * 
 * Voluntarily gives up the CPU to allow other processes to run.
 */
void process_yield(void);

/**
 * Find a zombie child process
 * 
 * @param parent Parent process
 * @param target_pid PID to search for (-1 for any child)
 * @return Zombie child process, or NULL if none found
 */
struct process *process_find_zombie_child(struct process *parent, int target_pid);

/**
 * Check if parent has any children matching criteria
 * 
 * @param parent Parent process
 * @param target_pid PID to search for (-1 for any child)
 * @return 1 if children exist, 0 otherwise
 */
int process_has_children(struct process *parent, int target_pid);

/**
 * Get process by index in process table
 * 
 * Used for iterating through all processes.
 * 
 * @param index Index in process table (0 to MAX_PROCS-1)
 * @return Process pointer, or NULL if index invalid or slot unused
 */
struct process *process_get_by_index(int index);

/**
 * Get maximum number of processes
 * 
 * @return Maximum process count (MAX_PROCS)
 */
int process_get_max_count(void);

/**
 * Sleep for a number of ticks
 * 
 * @param ticks Number of timer ticks to sleep
 */
void process_sleep(uint64_t ticks);

/**
 * Wake up a process
 * 
 * @param proc Process to wake up
 */
void process_wakeup(struct process *proc);

/**
 * Allocate a new PID
 * 
 * @return New PID, or -1 on failure
 */
pid_t alloc_pid(void);

/**
 * Free a process structure
 * 
 * @param proc Process to free
 */
void process_free(struct process *proc);

/**
 * Dump process table (for debugging)
 */
void process_dump(void);

/**
 * Fork the current process
 * 
 * Creates a copy of the current process.
 * 
 * @return Child PID in parent, 0 in child, -1 on error
 */
struct trap_frame;
pid_t process_fork(struct trap_frame *current_tf);

/**
 * Execute a new program in the current process
 * 
 * @param entry_point New entry point
 * @param arg Argument to pass to entry point
 * @return -1 on error (never returns on success)
 */
int process_exec(void (*entry_point)(void *), void *arg);

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
struct process *process_create_user(const char *name, void *user_code, size_t code_size);

/**
 * Create a new user process from loaded ELF segments
 * 
 * This function is similar to process_create_user but allows specifying
 * custom virtual address base and entry point for loaded ELF programs.
 * 
 * @param name Process name
 * @param code_base Virtual address base where code should be mapped
 * @param code_mem Physical address of loaded code (page-aligned)
 * @param code_size Size of code in bytes
 * @param entry_point Entry point virtual address
 * @return Pointer to new process, or NULL on failure
 */
struct process *process_create_elf(const char *name, uint64_t code_base, 
                                   void *code_mem, size_t code_size, 
                                   uint64_t entry_point);

/**
 * Return to user mode (assembly function)
 * 
 * This function restores all registers from the trap frame and executes
 * sret to enter user mode. Must be called with interrupts disabled.
 * 
 * @param trap_frame Pointer to trap frame with register state
 */
void user_return(struct trap_frame *trap_frame) __attribute__((noreturn));

/**
 * Allocate memory for a process segment
 * 
 * @param proc Process to allocate memory for
 * @param vaddr Virtual address to map
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL on failure
 */
void *process_alloc_mem(struct process *proc, uint64_t vaddr, uint64_t size);

/**
 * Set up process arguments (argc, argv)
 * 
 * @param proc Process to set up
 * @param argv Array of argument strings
 * @param argc Argument count
 */
void process_setup_args(struct process *proc, const char *argv[], int argc);

/**
 * Mark process as ready to run
 * 
 * @param proc Process to mark as ready
 */
void process_ready(struct process *proc);

/**
 * Set up memory isolation for a process
 * 
 * Initializes VMA list and heap boundaries.
 * 
 * @param proc Process to setup
 * @return 0 on success, -1 on failure
 */
int process_setup_memory_isolation(struct process *proc);

/**
 * Map a memory region in process address space
 * 
 * @param proc Process to map in
 * @param vaddr Virtual address to map
 * @param size Size in bytes
 * @param flags Protection flags (VM_READ, VM_WRITE, VM_EXEC, VM_USER)
 * @return 0 on success, -1 on failure
 */
int process_map_region(struct process *proc, uint64_t vaddr, uint64_t size, uint32_t flags);

/**
 * Find VMA containing an address
 * 
 * @param proc Process to search
 * @param addr Virtual address to find
 * @return Pointer to VMA, or NULL if not found
 */
vm_area_t *process_find_vma(struct process *proc, uint64_t addr);

/**
 * Add a VMA to process address space
 * 
 * @param proc Process to add VMA to
 * @param start Start address (inclusive)
 * @param end End address (exclusive)
 * @param flags Protection flags
 * @return 0 on success, -1 on failure
 */
int process_add_vma(struct process *proc, uint64_t start, uint64_t end, uint32_t flags);

/**
 * Remove a VMA from process address space
 * 
 * @param proc Process to remove VMA from
 * @param vma VMA to remove
 */
void process_remove_vma(struct process *proc, vm_area_t *vma);

/**
 * Clean up all VMAs for a process
 * 
 * @param proc Process to cleanup
 */
void process_cleanup_vmas(struct process *proc);

/**
 * Validate user pointer against process VMAs
 * 
 * @param proc Process to validate for
 * @param ptr Pointer to validate
 * @param size Size of memory region
 * @param required_flags Flags that must be set (e.g., VM_WRITE for write access)
 * @return 1 if valid, 0 if invalid
 */
int process_validate_user_ptr(struct process *proc, const void *ptr, size_t size, uint32_t required_flags);

/**
 * Set the controlling terminal for a process
 * 
 * @param proc Process to set terminal for
 * @param tty_index Terminal index (0 to VTERM_MAX_TERMINALS-1), or -1 for none
 * @return 0 on success, -1 on error
 */
int process_set_tty(struct process *proc, int tty_index);

/**
 * Get the controlling terminal for a process
 * 
 * @param proc Process to get terminal for
 * @return Terminal index, or -1 if none
 */
int process_get_tty(struct process *proc);

#endif // PROCESS_H
