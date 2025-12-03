/**
 * @file wait_queue.h
 * @brief Wait queue implementation for blocking I/O
 *
 * Wait queues allow processes to sleep efficiently while waiting for
 * events (data available, space available, etc.) rather than busy-waiting.
 *
 * Usage:
 *   wait_queue_t wq;
 *   wait_queue_init(&wq);
 *
 *   // In reader:
 *   while (no_data) {
 *       wait_queue_sleep(&wq);
 *   }
 *
 *   // In writer:
 *   add_data();
 *   wait_queue_wake(&wq);  // Wake all waiters
 */

#ifndef WAIT_QUEUE_H
#define WAIT_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/process.h"

/**
 * Maximum number of processes that can wait on a single queue
 */
#define WAIT_QUEUE_MAX_WAITERS 16

/**
 * Wait queue entry - represents one waiting process
 */
typedef struct wait_queue_entry {
    struct process *proc;           /**< Process waiting on this queue */
    struct wait_queue_entry *next;  /**< Next entry in the queue */
} wait_queue_entry_t;

/**
 * Wait queue structure
 *
 * A simple linked list of processes waiting for an event.
 * Processes are added when they sleep and removed when woken.
 */
typedef struct wait_queue {
    wait_queue_entry_t *head;  /**< First waiting process */
    wait_queue_entry_t *tail;  /**< Last waiting process (for O(1) append) */
    uint32_t count;            /**< Number of waiting processes */
} wait_queue_t;

/**
 * Static initializer for wait queue
 */
#define WAIT_QUEUE_INIT { .head = NULL, .tail = NULL, .count = 0 }

/**
 * Initialize a wait queue
 *
 * @param wq Pointer to wait queue to initialize
 */
void wait_queue_init(wait_queue_t *wq);

/**
 * Sleep on a wait queue
 *
 * Puts the current process to sleep and adds it to the wait queue.
 * The process will remain sleeping until woken by wait_queue_wake()
 * or wait_queue_wake_one().
 *
 * IMPORTANT: Caller should check the condition again after this
 * function returns, as spurious wakeups can occur.
 *
 * @param wq Pointer to wait queue
 */
void wait_queue_sleep(wait_queue_t *wq);

/**
 * Wake all processes sleeping on a wait queue
 *
 * Moves all waiting processes from SLEEPING to READY state
 * and adds them to the scheduler's ready queue.
 *
 * @param wq Pointer to wait queue
 * @return Number of processes woken
 */
int wait_queue_wake(wait_queue_t *wq);

/**
 * Wake one process sleeping on a wait queue
 *
 * Wakes the first (oldest) waiting process. Useful when only
 * one process can proceed (e.g., single consumer).
 *
 * @param wq Pointer to wait queue
 * @return 1 if a process was woken, 0 if queue was empty
 */
int wait_queue_wake_one(wait_queue_t *wq);

/**
 * Check if wait queue is empty
 *
 * @param wq Pointer to wait queue
 * @return 1 if empty, 0 if processes are waiting
 */
int wait_queue_empty(wait_queue_t *wq);

/**
 * Get number of waiting processes
 *
 * @param wq Pointer to wait queue
 * @return Number of processes waiting
 */
uint32_t wait_queue_count(wait_queue_t *wq);

/**
 * Remove a specific process from a wait queue
 *
 * Used when a process is terminated while waiting, or when
 * a timeout expires. Does nothing if process is not in queue.
 *
 * @param wq Pointer to wait queue
 * @param proc Process to remove
 * @return 1 if removed, 0 if not found
 */
int wait_queue_remove(wait_queue_t *wq, struct process *proc);

#endif /* WAIT_QUEUE_H */
