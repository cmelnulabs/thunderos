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
    
    // Memory management
    page_table_t *page_table;           // Virtual memory page table
    uintptr_t kernel_stack;             // Kernel stack base
    uintptr_t user_stack;               // User stack base (virtual)
    
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
pid_t process_fork(void);

/**
 * Execute a new program in the current process
 * 
 * @param entry_point New entry point
 * @param arg Argument to pass to entry point
 * @return -1 on error (never returns on success)
 */
int process_exec(void (*entry_point)(void *), void *arg);

#endif // PROCESS_H
