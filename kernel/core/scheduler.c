/*
 * Process Scheduler Implementation
 * 
 * Round-robin scheduler with priority support.
 */

#include "kernel/scheduler.h"
#include "kernel/process.h"
#include "kernel/config.h"
#include "kernel/panic.h"
#include "hal/hal_uart.h"
#include "arch/interrupt.h"

// Simple circular queue for ready processes
#define READY_QUEUE_SIZE MAX_PROCS
static struct process *ready_queue[READY_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

// Scheduler lock
static volatile int sched_lock = 0;

// Time slice for round-robin scheduling
// Calculated based on TIMER_INTERVAL_US from config.h
// Desired time slice: 1 second = 1,000,000 microseconds
// TIME_SLICE = 1,000,000 / TIMER_INTERVAL_US ticks
#define TIME_SLICE (1000000 / TIMER_INTERVAL_US)
static uint64_t current_time_slice = 0;

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
 * Initialize the scheduler
 */
void scheduler_init(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    current_time_slice = TIME_SLICE;
    
    hal_uart_puts("[OK] Scheduler initialized\n");
}

/**
 * Add a process to the ready queue
 */
void scheduler_enqueue(struct process *proc) {
    if (!proc) return;
    
    lock_acquire(&sched_lock);
    
    if (queue_count >= READY_QUEUE_SIZE) {
        hal_uart_puts("Warning: Ready queue full!\n");
        lock_release(&sched_lock);
        return;
    }
    
    int old_count = queue_count;
    int old_tail = queue_tail;
    
    ready_queue[queue_tail] = proc;
    queue_tail = (queue_tail + 1) % READY_QUEUE_SIZE;
    queue_count++;
    
    lock_release(&sched_lock);
}

/**
 * Remove a process from the ready queue
 */
void scheduler_dequeue(struct process *proc) {
    if (!proc) return;
    
    lock_acquire(&sched_lock);
    
    // Simple linear search and removal
    for (int i = 0; i < queue_count; i++) {
        int idx = (queue_head + i) % READY_QUEUE_SIZE;
        if (ready_queue[idx] == proc) {
            // Shift remaining elements
            for (int j = i; j < queue_count - 1; j++) {
                int curr = (queue_head + j) % READY_QUEUE_SIZE;
                int next = (queue_head + j + 1) % READY_QUEUE_SIZE;
                ready_queue[curr] = ready_queue[next];
            }
            queue_tail = (queue_tail - 1 + READY_QUEUE_SIZE) % READY_QUEUE_SIZE;
            queue_count--;
            break;
        }
    }
    
    lock_release(&sched_lock);
}

/**
 * Get the next process to run (round-robin)
 */
struct process *scheduler_pick_next(void) {
    lock_acquire(&sched_lock);
    
    if (queue_count == 0) {
        lock_release(&sched_lock);
        return NULL;
    }
    
    struct process *proc = ready_queue[queue_head];
    queue_head = (queue_head + 1) % READY_QUEUE_SIZE;
    queue_count--;
    
    lock_release(&sched_lock);
    
    return proc;
}

/**
 * External assembly function for context switching
 */
extern void context_switch_asm(struct context *old, struct context *new);

/**
 * External function to set current process
 */
extern void process_set_current(struct process *proc);

/**
 * Perform a context switch from old process to new process
 * 
 * NOTE: This function MUST be called with interrupts disabled
 * to ensure atomic state updates and prevent race conditions.
 */
void context_switch(struct process *old, struct process *new) {
    if (!new) {
        kernel_panic("context_switch: Attempting to switch to NULL process");
    }
    
    // Update states (interrupts must be disabled by caller)
    if (old && old->state == PROC_RUNNING) {
        old->state = PROC_READY;
    }
    new->state = PROC_RUNNING;
    
    // Set current process
    process_set_current(new);
    
    // Perform low-level context switch
    if (old) {
        context_switch_asm(&old->context, &new->context);
    } else {
        context_switch_asm(NULL, &new->context);
    }
    
    // NOW switch page tables AFTER context switch completes
    // This is safe because we're now executing with the new process's kernel stack
    switch_page_table(new->page_table);
}

/**
 * Schedule next process to run
 * 
 * This function is called by:
 * 1. Timer interrupt (preemptive)
 * 2. process_yield() (voluntary)
 * 3. process_exit() (termination)
 */
void schedule(void) {
    // Disable interrupts during scheduling
    int old_state = interrupt_save_disable();
    
    struct process *current = process_current();
    struct process *next = NULL;
    
    // Decrement time slice
    if (current_time_slice > 0) {
        current_time_slice--;
    }
    
    // Check if we should preempt current process
    int should_preempt = 0;
    
    if (!current) {
        // No current process, pick next
        should_preempt = 1;
    } else if (current->state != PROC_RUNNING) {
        // Current process is not running (sleeping, zombie, etc.)
        should_preempt = 1;
    } else if (current_time_slice == 0) {
        // Time slice expired
        should_preempt = 1;
        current_time_slice = TIME_SLICE;
    }
    
    if (should_preempt) {
        // Pick next process
        next = scheduler_pick_next();
        
        if (!next) {
            // No process to run, use current if available
            if (current && current->state == PROC_RUNNING) {
                next = current;
            } else {
                // Idle - no process to run
                // If current process is not running (zombie, etc.), switch back to kernel page table
                if (current && current->state != PROC_RUNNING) {
                    extern void switch_to_kernel_page_table(void);
                    switch_to_kernel_page_table();
                    process_set_current(NULL);
                }
                // Re-enable interrupts and halt
                interrupt_restore(old_state);
                return;
            }
        }
        
        // Add current process back to ready queue if still ready
        if (current && current->state == PROC_RUNNING && current != next) {
            scheduler_enqueue(current);
        }
        
        // Switch to next process
        if (next != current) {
            context_switch(current, next);
        }
    }
    
    // Restore interrupt state
    interrupt_restore(old_state);
}

/**
 * Voluntarily yield CPU to another process
 * 
 * Current process gives up its CPU time slice and scheduler
 * picks the next process to run. Useful for cooperative multitasking
 * and when waiting for child processes.
 */
void scheduler_yield(void) {
    // Reset time slice to force scheduling
    current_time_slice = 0;
    
    // Call scheduler
    schedule();
}

/**
 * Helper function called from assembly to get current process trap frame
 */
struct trap_frame *process_get_current_trap_frame(void) {
    struct process *proc = process_current();
    return proc ? proc->trap_frame : NULL;
}
