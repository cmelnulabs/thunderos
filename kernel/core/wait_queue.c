/**
 * @file wait_queue.c
 * @brief Wait queue implementation for blocking I/O
 *
 * Implements sleep/wakeup mechanism for processes waiting on events.
 * This is the foundation for blocking pipes, sockets, and other I/O.
 */

#include "kernel/wait_queue.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/errno.h"
#include "mm/kmalloc.h"
#include "arch/interrupt.h"
#include "hal/hal_uart.h"

/**
 * Initialize a wait queue
 */
void wait_queue_init(wait_queue_t *wq) {
    if (!wq) return;
    
    wq->head = NULL;
    wq->tail = NULL;
    wq->count = 0;
}

/**
 * Sleep on a wait queue
 *
 * Adds current process to the wait queue and puts it to sleep.
 * The scheduler will not run this process until it's woken up.
 */
void wait_queue_sleep(wait_queue_t *wq) {
    if (!wq) return;
    
    struct process *current = process_current();
    if (!current) {
        // No current process - this shouldn't happen
        return;
    }
    
    // Disable interrupts to ensure atomic operation
    int old_state = interrupt_save_disable();
    
    // Allocate wait queue entry
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    if (!entry) {
        // Failed to allocate - can't sleep, return immediately
        interrupt_restore(old_state);
        return;
    }
    
    entry->proc = current;
    entry->next = NULL;
    
    // Add to tail of wait queue
    if (wq->tail) {
        wq->tail->next = entry;
        wq->tail = entry;
    } else {
        // Empty queue
        wq->head = entry;
        wq->tail = entry;
    }
    wq->count++;
    
    // Mark process as sleeping
    current->state = PROC_SLEEPING;
    
    // Remove from scheduler ready queue (if present)
    scheduler_dequeue(current);
    
    // Re-enable interrupts and yield to scheduler
    interrupt_restore(old_state);
    
    // Yield CPU - scheduler will pick another process
    // When we're woken up, schedule() will return here
    schedule();
    
    // We've been woken up - the entry was freed by wake function
}

/**
 * Wake all processes sleeping on a wait queue
 */
int wait_queue_wake(wait_queue_t *wq) {
    if (!wq) return 0;
    
    int woken = 0;
    
    // Disable interrupts for atomic operation
    int old_state = interrupt_save_disable();
    
    wait_queue_entry_t *entry = wq->head;
    while (entry) {
        wait_queue_entry_t *next = entry->next;
        
        struct process *proc = entry->proc;
        if (proc && proc->state == PROC_SLEEPING) {
            // Wake the process
            proc->state = PROC_READY;
            scheduler_enqueue(proc);
            woken++;
        }
        
        // Free the entry
        kfree(entry);
        entry = next;
    }
    
    // Clear the queue
    wq->head = NULL;
    wq->tail = NULL;
    wq->count = 0;
    
    interrupt_restore(old_state);
    
    return woken;
}

/**
 * Wake one process sleeping on a wait queue
 */
int wait_queue_wake_one(wait_queue_t *wq) {
    if (!wq || !wq->head) return 0;
    
    // Disable interrupts for atomic operation
    int old_state = interrupt_save_disable();
    
    // Get first entry
    wait_queue_entry_t *entry = wq->head;
    if (!entry) {
        interrupt_restore(old_state);
        return 0;
    }
    
    // Remove from queue
    wq->head = entry->next;
    if (!wq->head) {
        wq->tail = NULL;
    }
    wq->count--;
    
    // Wake the process
    struct process *proc = entry->proc;
    if (proc && proc->state == PROC_SLEEPING) {
        proc->state = PROC_READY;
        scheduler_enqueue(proc);
    }
    
    // Free the entry
    kfree(entry);
    
    interrupt_restore(old_state);
    
    return 1;
}

/**
 * Check if wait queue is empty
 */
int wait_queue_empty(wait_queue_t *wq) {
    if (!wq) return 1;
    return wq->count == 0;
}

/**
 * Get number of waiting processes
 */
uint32_t wait_queue_count(wait_queue_t *wq) {
    if (!wq) return 0;
    return wq->count;
}

/**
 * Remove a specific process from a wait queue
 */
int wait_queue_remove(wait_queue_t *wq, struct process *proc) {
    if (!wq || !proc) return 0;
    
    int old_state = interrupt_save_disable();
    
    wait_queue_entry_t *prev = NULL;
    wait_queue_entry_t *entry = wq->head;
    
    while (entry) {
        if (entry->proc == proc) {
            // Found it - remove from queue
            if (prev) {
                prev->next = entry->next;
            } else {
                wq->head = entry->next;
            }
            
            if (entry == wq->tail) {
                wq->tail = prev;
            }
            
            wq->count--;
            kfree(entry);
            
            interrupt_restore(old_state);
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    
    interrupt_restore(old_state);
    return 0;
}
