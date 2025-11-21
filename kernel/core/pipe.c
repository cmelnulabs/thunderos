/**
 * @file pipe.c
 * @brief Pipe (FIFO) inter-process communication implementation
 * 
 * Implements anonymous pipes for unidirectional communication between processes.
 * Uses a circular buffer for efficient data transfer without copying.
 */

#include "kernel/pipe.h"
#include "kernel/errno.h"
#include "kernel/process.h"
#include "mm/kmalloc.h"
#include "kernel/kstring.h"

/**
 * Initialize pipe subsystem
 * 
 * Currently no global initialization needed. Reserved for future use
 * (e.g., pipe cache, statistics).
 */
void pipe_init(void) {
    // No global initialization needed yet
}

/**
 * Create a new pipe
 * 
 * Allocates and initializes a pipe structure with empty circular buffer.
 * Both read and write ends start with reference count of 1.
 * 
 * @return Pointer to newly created pipe, or NULL on allocation failure
 */
pipe_t* pipe_create(void) {
    pipe_t* pipe = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!pipe) {
        set_errno(THUNDEROS_ENOMEM);
        return NULL;
    }

    // Initialize pipe to empty state
    kmemset(pipe->buffer, 0, PIPE_BUF_SIZE);
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->data_size = 0;
    pipe->state = PIPE_OPEN;
    pipe->read_ref_count = 1;
    pipe->write_ref_count = 1;

    clear_errno();
    return pipe;
}

/**
 * Read data from pipe
 * 
 * Reads up to 'count' bytes from pipe's circular buffer. Handles wraparound
 * automatically. Returns immediately if data is available, or 0 if write end
 * is closed (EOF), or -EAGAIN if pipe is empty but write end is open.
 * 
 * @param pipe Pointer to pipe structure
 * @param buffer Destination buffer
 * @param count Maximum bytes to read
 * @return Bytes read, 0 on EOF, -1 on error
 */
int pipe_read(pipe_t* pipe, void* buffer, size_t count) {
    if (!pipe || !buffer) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    // Check if read end is already closed
    if (pipe->state == PIPE_READ_CLOSED || pipe->state == PIPE_CLOSED) {
        RETURN_ERRNO(THUNDEROS_EPIPE);
    }

    // Wait for data to be available
    // TODO: Implement proper blocking with wakeup mechanism
    // For now, return EAGAIN if no data available (non-blocking)
    if (pipe->data_size == 0) {
        // If write end is closed, return EOF
        if (pipe->state == PIPE_WRITE_CLOSED || pipe->write_ref_count == 0) {
            clear_errno();
            return 0;  // EOF
        }
        // No data and write end still open
        RETURN_ERRNO(THUNDEROS_EAGAIN);
    }

    // Read available data (limited by count and available data)
    size_t to_read = count;
    if (to_read > pipe->data_size) {
        to_read = pipe->data_size;
    }

    char* dest = (char*)buffer;
    size_t bytes_read = 0;

    // Handle circular buffer wraparound
    while (bytes_read < to_read) {
        size_t chunk_size = to_read - bytes_read;
        size_t remaining_in_buffer = PIPE_BUF_SIZE - pipe->read_pos;
        
        if (chunk_size > remaining_in_buffer) {
            chunk_size = remaining_in_buffer;
        }

        kmemcpy(dest + bytes_read, pipe->buffer + pipe->read_pos, chunk_size);
        bytes_read += chunk_size;
        pipe->read_pos = (pipe->read_pos + chunk_size) % PIPE_BUF_SIZE;
    }

    pipe->data_size -= bytes_read;

    clear_errno();
    return (int)bytes_read;
}

/**
 * Write data to pipe
 * 
 * Writes up to 'count' bytes into pipe's circular buffer. Returns immediately
 * if space is available, or -EAGAIN if pipe is full, or -EPIPE if read end
 * is closed.
 * 
 * @param pipe Pointer to pipe structure
 * @param buffer Source buffer
 * @param count Bytes to write
 * @return Bytes written, -1 on error
 */
int pipe_write(pipe_t* pipe, const void* buffer, size_t count) {
    if (!pipe || !buffer) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    // Check if write end is already closed
    if (pipe->state == PIPE_WRITE_CLOSED || pipe->state == PIPE_CLOSED) {
        RETURN_ERRNO(THUNDEROS_EPIPE);
    }

    // Check if read end is closed (broken pipe)
    if (pipe->state == PIPE_READ_CLOSED || pipe->read_ref_count == 0) {
        RETURN_ERRNO(THUNDEROS_EPIPE);
    }

    // If pipe is full, return would-block
    if (pipe->data_size >= PIPE_BUF_SIZE) {
        RETURN_ERRNO(THUNDEROS_EAGAIN);
    }

    // Write available space (limited by count and free space)
    size_t free_space = PIPE_BUF_SIZE - pipe->data_size;
    size_t to_write = count;
    if (to_write > free_space) {
        to_write = free_space;
    }

    const char* src = (const char*)buffer;
    size_t bytes_written = 0;

    // Handle circular buffer wraparound
    while (bytes_written < to_write) {
        size_t chunk_size = to_write - bytes_written;
        size_t remaining_in_buffer = PIPE_BUF_SIZE - pipe->write_pos;
        
        if (chunk_size > remaining_in_buffer) {
            chunk_size = remaining_in_buffer;
        }

        kmemcpy(pipe->buffer + pipe->write_pos, src + bytes_written, chunk_size);
        bytes_written += chunk_size;
        pipe->write_pos = (pipe->write_pos + chunk_size) % PIPE_BUF_SIZE;
    }

    pipe->data_size += bytes_written;

    clear_errno();
    return (int)bytes_written;
}

/**
 * Close read end of pipe
 * 
 * Decrements read reference count and updates state if count reaches 0.
 * 
 * @param pipe Pointer to pipe structure
 * @return 0 on success, -1 on error
 */
int pipe_close_read(pipe_t* pipe) {
    if (!pipe) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    if (pipe->read_ref_count > 0) {
        pipe->read_ref_count--;
    }

    if (pipe->read_ref_count == 0) {
        if (pipe->state == PIPE_WRITE_CLOSED) {
            pipe->state = PIPE_CLOSED;
        } else {
            pipe->state = PIPE_READ_CLOSED;
        }
    }

    clear_errno();
    return 0;
}

/**
 * Close write end of pipe
 * 
 * Decrements write reference count and updates state if count reaches 0.
 * Subsequent reads will return EOF when buffer is drained.
 * 
 * @param pipe Pointer to pipe structure
 * @return 0 on success, -1 on error
 */
int pipe_close_write(pipe_t* pipe) {
    if (!pipe) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    if (pipe->write_ref_count > 0) {
        pipe->write_ref_count--;
    }

    if (pipe->write_ref_count == 0) {
        if (pipe->state == PIPE_READ_CLOSED) {
            pipe->state = PIPE_CLOSED;
        } else {
            pipe->state = PIPE_WRITE_CLOSED;
        }
    }

    clear_errno();
    return 0;
}

/**
 * Check if pipe can be freed
 * 
 * @param pipe Pointer to pipe structure
 * @return 1 if both ends closed, 0 otherwise
 */
int pipe_can_free(pipe_t* pipe) {
    if (!pipe) {
        return 0;
    }
    return pipe->state == PIPE_CLOSED;
}

/**
 * Free pipe resources
 * 
 * Deallocates pipe structure. Caller should verify both ends are closed first.
 * 
 * @param pipe Pointer to pipe structure
 */
void pipe_free(pipe_t* pipe) {
    if (pipe) {
        kfree(pipe);
    }
}
