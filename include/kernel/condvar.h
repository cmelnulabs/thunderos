/**
 * @file condvar.h
 * @brief Condition variable synchronization primitive for ThunderOS
 *
 * Provides condition variables for coordinating threads/processes.
 * Condition variables allow threads to wait for certain conditions to become
 * true and be signaled when those conditions are met.
 */

#ifndef _KERNEL_CONDVAR_H
#define _KERNEL_CONDVAR_H

#include <stdint.h>
#include "kernel/wait_queue.h"
#include "kernel/mutex.h"

/**
 * @brief Condition variable structure
 *
 * A condition variable allows processes to wait for a condition to become true.
 * It must always be used with a mutex to avoid race conditions.
 *
 * Typical usage pattern:
 *   mutex_lock(&mutex);
 *   while (!condition) {
 *       cond_wait(&cv, &mutex);  // Atomically unlocks mutex and sleeps
 *   }
 *   // condition is now true, mutex is locked
 *   // do work...
 *   mutex_unlock(&mutex);
 */
typedef struct condvar {
    wait_queue_t waiters;         /**< Processes waiting on this condition */
} condvar_t;

/**
 * @brief Static initializer for condition variable
 */
#define CONDVAR_INIT { .waiters = WAIT_QUEUE_INIT }

/**
 * @brief Initialize a condition variable
 *
 * @param cv Pointer to condition variable to initialize
 */
void cond_init(condvar_t *cv);

/**
 * @brief Wait on a condition variable (blocking)
 *
 * Atomically unlocks the mutex and puts the calling process to sleep
 * on the condition variable's wait queue. When awakened (by cond_signal
 * or cond_broadcast), the mutex is automatically re-acquired before returning.
 *
 * The mutex must be locked by the caller before calling this function.
 *
 * This function must be called in a loop that checks the actual condition,
 * as spurious wakeups can occur.
 *
 * @param cv Pointer to condition variable to wait on
 * @param mutex Pointer to mutex associated with this condition (must be locked)
 */
void cond_wait(condvar_t *cv, mutex_t *mutex);

/**
 * @brief Signal one waiting process
 *
 * Wakes up one process waiting on the condition variable (if any).
 * The caller should hold the associated mutex when calling this function
 * (though it's not strictly required by the implementation).
 *
 * @param cv Pointer to condition variable to signal
 */
void cond_signal(condvar_t *cv);

/**
 * @brief Broadcast to all waiting processes
 *
 * Wakes up all processes waiting on the condition variable.
 * The caller should hold the associated mutex when calling this function
 * (though it's not strictly required by the implementation).
 *
 * @param cv Pointer to condition variable to broadcast
 */
void cond_broadcast(condvar_t *cv);

/**
 * @brief Destroy a condition variable
 *
 * Cleans up a condition variable. No processes should be waiting on it.
 *
 * @param cv Pointer to condition variable to destroy
 */
void cond_destroy(condvar_t *cv);

#endif /* _KERNEL_CONDVAR_H */
