/**
 * @file pipe.h
 * @brief Pipe (FIFO) inter-process communication
 * 
 * Provides anonymous pipes for unidirectional communication between processes.
 * Pipes are typically used with fork() to enable parent-child communication or
 * shell pipelines (e.g., "cat file.txt | grep pattern").
 * 
 * A pipe has two ends:
 * - Read end (fd[0]): Blocks if no data available, returns EOF when write end closed
 * - Write end (fd[1]): Blocks if buffer full, returns error if read end closed
 * 
 * Maximum pipe buffer size is 4KB (one page).
 */

#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>
#include "kernel/wait_queue.h"

/**
 * Maximum size of pipe buffer (4KB - one page)
 */
#define PIPE_BUF_SIZE 4096

/**
 * Pipe states
 */
#define PIPE_OPEN       0  /**< Pipe is open and operational */
#define PIPE_READ_CLOSED  1  /**< Read end has been closed */
#define PIPE_WRITE_CLOSED 2  /**< Write end has been closed */
#define PIPE_CLOSED     3  /**< Both ends closed, can be freed */

/**
 * Pipe structure
 * 
 * Contains circular buffer for data, read/write positions, reference counts
 * for tracking when both ends are closed, and wait queues for blocking I/O.
 */
typedef struct pipe {
    char buffer[PIPE_BUF_SIZE];  /**< Circular buffer for pipe data */
    uint32_t read_pos;           /**< Current read position in buffer */
    uint32_t write_pos;          /**< Current write position in buffer */
    uint32_t data_size;          /**< Number of bytes currently in buffer */
    uint32_t state;              /**< Current pipe state (PIPE_*) */
    uint32_t read_ref_count;     /**< Number of open read ends */
    uint32_t write_ref_count;    /**< Number of open write ends */
    wait_queue_t readers;        /**< Processes waiting to read (pipe empty) */
    wait_queue_t writers;        /**< Processes waiting to write (pipe full) */
} pipe_t;

/**
 * Initialize pipe subsystem
 * 
 * Must be called during kernel initialization before any pipes can be created.
 */
void pipe_init(void);

/**
 * Create a new pipe
 * 
 * Allocates a pipe structure and initializes it to empty state.
 * The pipe has both read and write ends open with reference counts of 1.
 * 
 * @return Pointer to newly created pipe, or NULL on allocation failure
 * 
 * @errno THUNDEROS_ENOMEM - Failed to allocate memory for pipe
 */
pipe_t* pipe_create(void);

/**
 * Read data from pipe (blocking)
 * 
 * Reads up to 'count' bytes from the pipe into the buffer. If the pipe is empty:
 * - Blocks (sleeps) if write end is still open, until data arrives
 * - Returns 0 if write end is closed (EOF)
 * 
 * @param pipe Pointer to pipe structure
 * @param buffer Destination buffer for read data
 * @param count Maximum number of bytes to read
 * 
 * @return Number of bytes read on success, 0 on EOF, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid pipe or buffer pointer
 * @errno THUNDEROS_EPIPE - Pipe read end already closed
 */
int pipe_read(pipe_t* pipe, void* buffer, size_t count);

/**
 * Write data to pipe (blocking)
 * 
 * Writes up to 'count' bytes from buffer into the pipe. If pipe is full:
 * - Blocks (sleeps) until space is available
 * 
 * If read end is closed, returns error immediately (broken pipe).
 * 
 * @param pipe Pointer to pipe structure
 * @param buffer Source buffer containing data to write
 * @param count Number of bytes to write
 * 
 * @return Number of bytes written on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid pipe or buffer pointer
 * @errno THUNDEROS_EPIPE - Read end closed, cannot write (broken pipe)
 */
int pipe_write(pipe_t* pipe, const void* buffer, size_t count);

/**
 * Close read end of pipe
 * 
 * Decrements read reference count. If count reaches 0, marks read end as closed.
 * If both ends are closed, the pipe can be freed.
 * 
 * @param pipe Pointer to pipe structure
 * 
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid pipe pointer
 */
int pipe_close_read(pipe_t* pipe);

/**
 * Close write end of pipe
 * 
 * Decrements write reference count. If count reaches 0, marks write end as closed.
 * Any subsequent read on this pipe will return EOF when buffer is empty.
 * If both ends are closed, the pipe can be freed.
 * 
 * @param pipe Pointer to pipe structure
 * 
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid pipe pointer
 */
int pipe_close_write(pipe_t* pipe);

/**
 * Check if pipe can be freed
 * 
 * A pipe can be freed when both read and write ends are closed.
 * 
 * @param pipe Pointer to pipe structure
 * @return 1 if pipe can be freed, 0 otherwise
 */
int pipe_can_free(pipe_t* pipe);

/**
 * Free pipe resources
 * 
 * Deallocates pipe structure. Should only be called when both ends are closed.
 * 
 * @param pipe Pointer to pipe structure to free
 */
void pipe_free(pipe_t* pipe);

#endif /* PIPE_H */
