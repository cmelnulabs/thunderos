/**
 * @file condvar.c
 * @brief Condition variable implementation for ThunderOS
 *
 * Implements condition variables for coordinating threads/processes.
 * Condition variables work with mutexes to provide wait/signal semantics.
 */

#include "kernel/condvar.h"
#include "kernel/mutex.h"
#include "kernel/wait_queue.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/errno.h"
#include "arch/interrupt.h"

/**
 * @brief Initialize a condition variable
 *
 * Sets up an empty wait queue for processes waiting on this condition.
 *
 * @param cv Pointer to condition variable to initialize
 */
void cond_init(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    wait_queue_init(&cv->waiters);
}

/**
 * @brief Wait on a condition variable (blocking)
 *
 * This is the core condition variable operation. It performs the following
 * atomic sequence:
 * 1. Unlock the provided mutex
 * 2. Put the calling process to sleep on the condition variable's wait queue
 * 3. When awakened, re-acquire the mutex before returning
 *
 * The atomicity is crucial to avoid the "lost wakeup" problem where a signal
 * arrives between unlocking the mutex and sleeping.
 *
 * @param cv Pointer to condition variable to wait on
 * @param mutex Pointer to mutex associated with this condition (must be locked)
 */
void cond_wait(condvar_t *cv, mutex_t *mutex) {
    if (!cv || !mutex) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* 
     * Atomically unlock the mutex and add ourselves to the wait queue.
     * This is the critical section that prevents lost wakeups.
     */
    
    /* Unlock the mutex - this allows other processes to proceed */
    mutex->locked = MUTEX_UNLOCKED;
    mutex->owner_pid = -1;
    
    /* Wake one process waiting on the mutex (if any) */
    wait_queue_wake(&mutex->waiters);
    
    /* 
     * Now sleep on the condition variable.
     * We're still holding interrupts disabled, so no wakeup can be lost.
     * The wait_queue_sleep function will restore interrupts and yield.
     */
    interrupt_restore(flags);
    wait_queue_sleep(&cv->waiters);
    
    /* 
     * We've been awakened! Now we need to re-acquire the mutex.
     * This blocks if another process has taken it.
     */
    mutex_lock(mutex);
    
    /* 
     * Mutex is now locked, we can return to the caller.
     * The caller should re-check the condition in a while loop.
     */
}

/**
 * @brief Signal one waiting process
 *
 * Wakes up one process waiting on the condition variable. If no processes
 * are waiting, this is a no-op. The awakened process will re-acquire the
 * mutex before cond_wait() returns.
 *
 * Best practice: Call this while holding the associated mutex to ensure
 * atomicity of condition changes and signaling.
 *
 * @param cv Pointer to condition variable to signal
 */
void cond_signal(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wake one waiting process (if any) */
    wait_queue_wake(&cv->waiters);
    
    interrupt_restore(flags);
}

/**
 * @brief Broadcast to all waiting processes
 *
 * Wakes up all processes waiting on the condition variable. All awakened
 * processes will compete to re-acquire the mutex. This is typically used
 * when a condition change may satisfy multiple waiters.
 *
 * Best practice: Call this while holding the associated mutex to ensure
 * atomicity of condition changes and broadcasting.
 *
 * @param cv Pointer to condition variable to broadcast
 */
void cond_broadcast(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wake all waiting processes */
    wait_queue_wake(&cv->waiters);
    
    interrupt_restore(flags);
}

/**
 * @brief Destroy a condition variable
 *
 * Cleans up a condition variable. In our implementation, this is currently
 * a no-op since we don't dynamically allocate resources. However, it's
 * good practice to call this for symmetry with cond_init().
 *
 * Warning: No processes should be waiting on the condition variable when
 * this is called, or they may never wake up.
 *
 * @param cv Pointer to condition variable to destroy
 */
void cond_destroy(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    /* 
     * In a more sophisticated implementation, we might:
     * - Check that the wait queue is empty
     * - Wake any remaining waiters with an error
     * - Free any dynamically allocated resources
     * 
     * For now, this is a no-op.
     */
}
