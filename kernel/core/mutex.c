/**
 * @file mutex.c
 * @brief Mutex and semaphore implementation for ThunderOS
 *
 * Implements mutual exclusion and counting semaphore primitives using
 * wait queues for blocking synchronization.
 */

#include "kernel/mutex.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/errno.h"
#include "kernel/constants.h"
#include "arch/interrupt.h"

/* Forward declaration to avoid circular includes */
struct process;

/**
 * @brief Initialize a mutex
 *
 * Sets the mutex to unlocked state with no owner.
 *
 * @param mutex Pointer to mutex to initialize
 */
void mutex_init(mutex_t *mutex) {
    if (!mutex) {
        return;
    }
    
    mutex->locked = MUTEX_UNLOCKED;
    mutex->owner_pid = -1;
    wait_queue_init(&mutex->waiters);
}

/**
 * @brief Acquire a mutex (blocking)
 *
 * Atomically checks if the mutex is unlocked and acquires it.
 * If the mutex is already held, the calling process is added to the
 * wait queue and put to sleep until the mutex becomes available.
 *
 * @param mutex Pointer to mutex to acquire
 */
void mutex_lock(mutex_t *mutex) {
    if (!mutex) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Spin until we can acquire the lock */
    while (mutex->locked == MUTEX_LOCKED) {
        /* Add ourselves to the wait queue and sleep */
        interrupt_restore(flags);
        wait_queue_sleep(&mutex->waiters);
        flags = interrupt_save_disable();
    }
    
    /* We got the lock */
    mutex->locked = MUTEX_LOCKED;
    
    /* Record owner (get current process PID) */
    struct process *current = process_current();
    if (current) {
        mutex->owner_pid = current->pid;
    }
    
    interrupt_restore(flags);
}

/**
 * @brief Try to acquire a mutex (non-blocking)
 *
 * Attempts to acquire the mutex without blocking. Returns immediately
 * with success or failure.
 *
 * @param mutex Pointer to mutex to try to acquire
 * @return 0 if lock acquired, -1 if mutex was already locked
 */
int mutex_trylock(mutex_t *mutex) {
    if (!mutex) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    uint64_t flags = interrupt_save_disable();
    
    if (mutex->locked == MUTEX_LOCKED) {
        interrupt_restore(flags);
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    /* Acquire the lock */
    mutex->locked = MUTEX_LOCKED;
    
    struct process *current = process_current();
    if (current) {
        mutex->owner_pid = current->pid;
    }
    
    interrupt_restore(flags);
    clear_errno();
    return 0;
}

/**
 * @brief Release a mutex
 *
 * Releases the mutex and wakes one waiting process if any.
 * Should only be called by the process that holds the lock.
 *
 * @param mutex Pointer to mutex to release
 */
void mutex_unlock(mutex_t *mutex) {
    if (!mutex) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Release the lock */
    mutex->locked = MUTEX_UNLOCKED;
    mutex->owner_pid = -1;
    
    interrupt_restore(flags);
    
    /* Wake one waiting process */
    wait_queue_wake_one(&mutex->waiters);
}

/**
 * @brief Check if mutex is locked
 *
 * @param mutex Pointer to mutex to check
 * @return 1 if locked, 0 if unlocked
 */
int mutex_is_locked(mutex_t *mutex) {
    if (!mutex) {
        return 0;
    }
    return mutex->locked == MUTEX_LOCKED;
}

/* ========================================================================
 * Semaphore Implementation
 * ======================================================================== */

/**
 * @brief Initialize a semaphore
 *
 * @param sem Pointer to semaphore to initialize
 * @param initial_count Initial count value (typically >= 0)
 */
void semaphore_init(semaphore_t *sem, int initial_count) {
    if (!sem) {
        return;
    }
    
    sem->count = initial_count;
    wait_queue_init(&sem->waiters);
}

/**
 * @brief Wait on a semaphore (P operation, decrement)
 *
 * Decrements the semaphore count. If the count is zero or negative,
 * blocks until another process signals the semaphore.
 *
 * @param sem Pointer to semaphore to wait on
 */
void semaphore_wait(semaphore_t *sem) {
    if (!sem) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wait while count is zero or negative */
    while (sem->count <= 0) {
        interrupt_restore(flags);
        wait_queue_sleep(&sem->waiters);
        flags = interrupt_save_disable();
    }
    
    /* Decrement count */
    sem->count--;
    
    interrupt_restore(flags);
}

/**
 * @brief Try to wait on a semaphore (non-blocking)
 *
 * Attempts to decrement the semaphore without blocking.
 *
 * @param sem Pointer to semaphore
 * @return 0 if decremented successfully, -1 if would block
 */
int semaphore_trywait(semaphore_t *sem) {
    if (!sem) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    uint64_t flags = interrupt_save_disable();
    
    if (sem->count <= 0) {
        interrupt_restore(flags);
        RETURN_ERRNO(THUNDEROS_EAGAIN);
    }
    
    sem->count--;
    
    interrupt_restore(flags);
    clear_errno();
    return 0;
}

/**
 * @brief Signal a semaphore (V operation, increment)
 *
 * Increments the semaphore count and wakes one waiting process if any.
 *
 * @param sem Pointer to semaphore to signal
 */
void semaphore_signal(semaphore_t *sem) {
    if (!sem) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Increment count */
    sem->count++;
    
    interrupt_restore(flags);
    
    /* Wake one waiting process */
    wait_queue_wake_one(&sem->waiters);
}

/**
 * @brief Get current semaphore count
 *
 * @param sem Pointer to semaphore
 * @return Current count value
 */
int semaphore_get_count(semaphore_t *sem) {
    if (!sem) {
        return 0;
    }
    return sem->count;
}
