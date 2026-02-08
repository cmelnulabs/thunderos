/**
 * @file rwlock.c
 * @brief Reader-writer lock implementation for ThunderOS
 *
 * Implements a reader-writer lock that allows multiple concurrent readers
 * or a single exclusive writer. Writers have priority to prevent starvation.
 */

#include "kernel/rwlock.h"
#include "kernel/errno.h"
#include "kernel/constants.h"
#include "arch/interrupt.h"

/**
 * @brief Initialize a reader-writer lock
 *
 * @param rw Pointer to rwlock to initialize
 */
void rwlock_init(rwlock_t *rw) {
    if (!rw) {
        return;
    }
    
    rw->readers = 0;
    rw->writer = 0;
    rw->writers_waiting = 0;
    wait_queue_init(&rw->reader_queue);
    wait_queue_init(&rw->writer_queue);
}

/**
 * @brief Acquire read lock (blocking)
 *
 * Blocks if a writer holds the lock or if writers are waiting.
 * This gives priority to writers to prevent writer starvation.
 *
 * @param rw Pointer to rwlock to acquire for reading
 */
void rwlock_read_lock(rwlock_t *rw) {
    if (!rw) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wait while a writer holds the lock or writers are waiting */
    while (rw->writer || rw->writers_waiting > 0) {
        interrupt_restore(flags);
        wait_queue_sleep(&rw->reader_queue);
        flags = interrupt_save_disable();
    }
    
    /* Acquired read lock */
    rw->readers++;
    
    interrupt_restore(flags);
}

/**
 * @brief Try to acquire read lock (non-blocking)
 *
 * @param rw Pointer to rwlock
 * @return 0 if lock acquired, -1 if would block
 */
int rwlock_read_trylock(rwlock_t *rw) {
    if (!rw) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Cannot acquire if writer holds or writers waiting */
    if (rw->writer || rw->writers_waiting > 0) {
        interrupt_restore(flags);
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    /* Acquired read lock */
    rw->readers++;
    
    interrupt_restore(flags);
    clear_errno();
    return 0;
}

/**
 * @brief Release read lock
 *
 * Decrements reader count. If this was the last reader and writers
 * are waiting, wakes one writer.
 *
 * @param rw Pointer to rwlock to release
 */
void rwlock_read_unlock(rwlock_t *rw) {
    if (!rw) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    if (rw->readers > 0) {
        rw->readers--;
    }
    
    /* If no more readers and writers waiting, wake a writer */
    if (rw->readers == 0 && rw->writers_waiting > 0) {
        interrupt_restore(flags);
        wait_queue_wake_one(&rw->writer_queue);
        return;
    }
    
    interrupt_restore(flags);
}

/**
 * @brief Acquire write lock (blocking)
 *
 * Blocks until all readers release and no other writer holds the lock.
 * Increments writers_waiting to give priority over new readers.
 *
 * @param rw Pointer to rwlock to acquire for writing
 */
void rwlock_write_lock(rwlock_t *rw) {
    if (!rw) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Indicate we're waiting - this blocks new readers */
    rw->writers_waiting++;
    
    /* Wait while readers hold the lock or another writer holds it */
    while (rw->readers > 0 || rw->writer) {
        interrupt_restore(flags);
        wait_queue_sleep(&rw->writer_queue);
        flags = interrupt_save_disable();
    }
    
    /* Acquired write lock */
    rw->writers_waiting--;
    rw->writer = 1;
    
    interrupt_restore(flags);
}

/**
 * @brief Try to acquire write lock (non-blocking)
 *
 * @param rw Pointer to rwlock
 * @return 0 if lock acquired, -1 if would block
 */
int rwlock_write_trylock(rwlock_t *rw) {
    if (!rw) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Cannot acquire if readers hold or writer holds */
    if (rw->readers > 0 || rw->writer) {
        interrupt_restore(flags);
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    /* Acquired write lock */
    rw->writer = 1;
    
    interrupt_restore(flags);
    clear_errno();
    return 0;
}

/**
 * @brief Release write lock
 *
 * Releases the write lock. Wakes all waiting readers first (if any),
 * otherwise wakes one waiting writer.
 *
 * @param rw Pointer to rwlock to release
 */
void rwlock_write_unlock(rwlock_t *rw) {
    if (!rw) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    rw->writer = 0;
    
    /* Prefer waking readers over writers for fairness */
    /* Wake all waiting readers */
    interrupt_restore(flags);
    wait_queue_wake(&rw->reader_queue);
    
    /* If no readers were waiting but writers are, wake one writer */
    flags = interrupt_save_disable();
    if (rw->readers == 0 && rw->writers_waiting > 0) {
        interrupt_restore(flags);
        wait_queue_wake_one(&rw->writer_queue);
        return;
    }
    
    interrupt_restore(flags);
}

/**
 * @brief Get number of active readers
 *
 * @param rw Pointer to rwlock
 * @return Number of readers currently holding the lock
 */
int rwlock_reader_count(rwlock_t *rw) {
    if (!rw) {
        return 0;
    }
    return rw->readers;
}

/**
 * @brief Check if write lock is held
 *
 * @param rw Pointer to rwlock
 * @return 1 if a writer holds the lock, 0 otherwise
 */
int rwlock_is_write_locked(rwlock_t *rw) {
    if (!rw) {
        return 0;
    }
    return rw->writer;
}
