/**
 * @file rwlock.h
 * @brief Reader-writer lock synchronization primitive for ThunderOS
 *
 * Provides a reader-writer lock that allows multiple concurrent readers
 * or a single exclusive writer. Writers have priority to prevent starvation.
 */

#ifndef _KERNEL_RWLOCK_H
#define _KERNEL_RWLOCK_H

#include <stdint.h>
#include "kernel/wait_queue.h"

/**
 * @brief Reader-writer lock structure
 *
 * A reader-writer lock allows:
 * - Multiple readers to hold the lock simultaneously (shared access)
 * - Only one writer at a time (exclusive access)
 * - Writers block new readers to prevent writer starvation
 *
 * The lock uses two wait queues: one for readers and one for writers.
 * Writers have priority - when a writer is waiting, new readers block.
 */
typedef struct rwlock {
    volatile int readers;         /**< Number of active readers (0 when writer holds) */
    volatile int writer;          /**< 1 if a writer holds the lock, 0 otherwise */
    volatile int writers_waiting; /**< Number of writers waiting (for priority) */
    wait_queue_t reader_queue;    /**< Readers waiting to acquire */
    wait_queue_t writer_queue;    /**< Writers waiting to acquire */
} rwlock_t;

/**
 * @brief Static initializer for rwlock
 */
#define RWLOCK_INIT { \
    .readers = 0, \
    .writer = 0, \
    .writers_waiting = 0, \
    .reader_queue = WAIT_QUEUE_INIT, \
    .writer_queue = WAIT_QUEUE_INIT \
}

/**
 * @brief Initialize a reader-writer lock
 *
 * @param rw Pointer to rwlock to initialize
 */
void rwlock_init(rwlock_t *rw);

/**
 * @brief Acquire read lock (blocking)
 *
 * Blocks if a writer holds the lock or if writers are waiting.
 * Multiple readers can hold the lock simultaneously.
 *
 * @param rw Pointer to rwlock to acquire for reading
 */
void rwlock_read_lock(rwlock_t *rw);

/**
 * @brief Try to acquire read lock (non-blocking)
 *
 * @param rw Pointer to rwlock
 * @return 0 if lock acquired, -1 if would block
 */
int rwlock_read_trylock(rwlock_t *rw);

/**
 * @brief Release read lock
 *
 * Decrements reader count. If this was the last reader and writers
 * are waiting, wakes one writer.
 *
 * @param rw Pointer to rwlock to release
 */
void rwlock_read_unlock(rwlock_t *rw);

/**
 * @brief Acquire write lock (blocking)
 *
 * Blocks until all readers release and no other writer holds the lock.
 * Only one writer can hold the lock at a time.
 *
 * @param rw Pointer to rwlock to acquire for writing
 */
void rwlock_write_lock(rwlock_t *rw);

/**
 * @brief Try to acquire write lock (non-blocking)
 *
 * @param rw Pointer to rwlock
 * @return 0 if lock acquired, -1 if would block
 */
int rwlock_write_trylock(rwlock_t *rw);

/**
 * @brief Release write lock
 *
 * Releases the write lock and wakes waiting readers or a writer.
 * Prefers waking all waiting readers, then one writer if no readers.
 *
 * @param rw Pointer to rwlock to release
 */
void rwlock_write_unlock(rwlock_t *rw);

/**
 * @brief Get number of active readers
 *
 * @param rw Pointer to rwlock
 * @return Number of readers currently holding the lock
 */
int rwlock_reader_count(rwlock_t *rw);

/**
 * @brief Check if write lock is held
 *
 * @param rw Pointer to rwlock
 * @return 1 if a writer holds the lock, 0 otherwise
 */
int rwlock_is_write_locked(rwlock_t *rw);

#endif /* _KERNEL_RWLOCK_H */
