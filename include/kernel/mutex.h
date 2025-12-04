/**
 * @file mutex.h
 * @brief Mutex and semaphore synchronization primitives for ThunderOS
 *
 * Provides mutual exclusion and counting semaphore primitives built on top
 * of wait queues for blocking synchronization.
 */

#ifndef _KERNEL_MUTEX_H
#define _KERNEL_MUTEX_H

#include <stdint.h>
#include "kernel/wait_queue.h"

/**
 * @brief Mutex state values
 */
#define MUTEX_UNLOCKED  0
#define MUTEX_LOCKED    1

/**
 * @brief Mutex structure for mutual exclusion
 *
 * A mutex provides mutual exclusion - only one process can hold the lock
 * at a time. Other processes attempting to lock will block until the mutex
 * is unlocked.
 */
typedef struct mutex {
    volatile int locked;          /**< Lock state: MUTEX_UNLOCKED or MUTEX_LOCKED */
    volatile int owner_pid;       /**< PID of process holding the lock (-1 if none) */
    wait_queue_t waiters;         /**< Processes waiting to acquire the mutex */
} mutex_t;

/**
 * @brief Static initializer for mutex
 */
#define MUTEX_INIT { .locked = MUTEX_UNLOCKED, .owner_pid = -1, .waiters = WAIT_QUEUE_INIT }

/**
 * @brief Semaphore structure for counting synchronization
 *
 * A semaphore maintains a count. Processes can decrement (wait/P) or
 * increment (signal/V) the count. If the count would go negative on
 * a wait, the process blocks until another process signals.
 */
typedef struct semaphore {
    volatile int count;           /**< Current semaphore count */
    wait_queue_t waiters;         /**< Processes waiting on the semaphore */
} semaphore_t;

/**
 * @brief Static initializer for semaphore with initial count
 */
#define SEMAPHORE_INIT(n) { .count = (n), .waiters = WAIT_QUEUE_INIT }

/**
 * @brief Initialize a mutex
 *
 * @param mutex Pointer to mutex to initialize
 */
void mutex_init(mutex_t *mutex);

/**
 * @brief Acquire a mutex (blocking)
 *
 * Blocks if the mutex is already held by another process.
 * Recursive locking by the same process is NOT allowed and will deadlock.
 *
 * @param mutex Pointer to mutex to acquire
 */
void mutex_lock(mutex_t *mutex);

/**
 * @brief Try to acquire a mutex (non-blocking)
 *
 * Attempts to acquire the mutex without blocking.
 *
 * @param mutex Pointer to mutex to try to acquire
 * @return 0 if lock acquired, -1 if mutex was already locked
 */
int mutex_trylock(mutex_t *mutex);

/**
 * @brief Release a mutex
 *
 * Releases the mutex and wakes one waiting process if any.
 * Must be called by the process that holds the lock.
 *
 * @param mutex Pointer to mutex to release
 */
void mutex_unlock(mutex_t *mutex);

/**
 * @brief Check if mutex is locked
 *
 * @param mutex Pointer to mutex to check
 * @return 1 if locked, 0 if unlocked
 */
int mutex_is_locked(mutex_t *mutex);

/**
 * @brief Initialize a semaphore
 *
 * @param sem Pointer to semaphore to initialize
 * @param initial_count Initial count value (typically >= 0)
 */
void semaphore_init(semaphore_t *sem, int initial_count);

/**
 * @brief Wait on a semaphore (P operation, decrement)
 *
 * Decrements the semaphore count. If the count is zero, blocks until
 * another process signals the semaphore.
 *
 * @param sem Pointer to semaphore to wait on
 */
void semaphore_wait(semaphore_t *sem);

/**
 * @brief Try to wait on a semaphore (non-blocking)
 *
 * Attempts to decrement the semaphore without blocking.
 *
 * @param sem Pointer to semaphore
 * @return 0 if decremented successfully, -1 if would block
 */
int semaphore_trywait(semaphore_t *sem);

/**
 * @brief Signal a semaphore (V operation, increment)
 *
 * Increments the semaphore count and wakes one waiting process if any.
 *
 * @param sem Pointer to semaphore to signal
 */
void semaphore_signal(semaphore_t *sem);

/**
 * @brief Get current semaphore count
 *
 * @param sem Pointer to semaphore
 * @return Current count value
 */
int semaphore_get_count(semaphore_t *sem);

#endif /* _KERNEL_MUTEX_H */
