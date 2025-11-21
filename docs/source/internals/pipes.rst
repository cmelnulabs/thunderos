Pipes
=====

Overview
--------

ThunderOS implements anonymous pipes for inter-process communication (IPC). Pipes provide unidirectional data flow between processes, typically used with ``fork()`` to enable parent-child communication or shell pipelines.

A pipe consists of two file descriptors:

- **Read end** (``pipefd[0]``): For reading data from the pipe
- **Write end** (``pipefd[1]``): For writing data to the pipe

Pipes use a circular buffer with a maximum capacity of 4KB (one memory page).

Architecture
------------

Pipe Structure
~~~~~~~~~~~~~~

Each pipe is represented by a ``pipe_t`` structure:

.. code-block:: c

   typedef struct pipe {
       char buffer[PIPE_BUF_SIZE];  // 4KB circular buffer
       uint32_t read_pos;           // Current read position
       uint32_t write_pos;          // Current write position
       uint32_t data_size;          // Bytes currently in buffer
       uint32_t state;              // Pipe state (PIPE_OPEN, etc.)
       uint32_t read_ref_count;     // Open read ends
       uint32_t write_ref_count;    // Open write ends
   } pipe_t;

The circular buffer allows efficient data transfer without copying. Read and write positions wrap around at ``PIPE_BUF_SIZE`` (4096 bytes).

Pipe States
~~~~~~~~~~~

Pipes can be in one of four states:

- ``PIPE_OPEN`` (0): Both ends open and operational
- ``PIPE_READ_CLOSED`` (1): Read end closed, writes will fail
- ``PIPE_WRITE_CLOSED`` (2): Write end closed, reads return EOF when buffer empty
- ``PIPE_CLOSED`` (3): Both ends closed, pipe can be freed

VFS Integration
~~~~~~~~~~~~~~~

Pipes are integrated into the Virtual Filesystem (VFS) layer:

- File descriptors with ``type == VFS_TYPE_PIPE`` represent pipe ends
- The ``vfs_file_t.pipe`` field points to the underlying ``pipe_t`` structure
- Regular VFS operations (``read``, ``write``, ``close``) work transparently with pipes

System Call
-----------

sys_pipe
~~~~~~~~

**Prototype:**

.. code-block:: c

   int pipe(int pipefd[2]);

**Description:**

Creates an anonymous pipe and returns two file descriptors in the ``pipefd`` array:

- ``pipefd[0]`` is the read end
- ``pipefd[1]`` is the write end

Data written to the write end can be read from the read end. Pipes are typically used after ``fork()`` to enable parent-child communication.

**Parameters:**

- ``pipefd[2]``: Array to receive the two file descriptors

**Return Value:**

- ``0`` on success
- ``-1`` on error (check ``errno``)

**Errors:**

- ``THUNDEROS_EINVAL``: Invalid ``pipefd`` pointer
- ``THUNDEROS_EMFILE``: Too many open files
- ``THUNDEROS_ENOMEM``: Failed to allocate pipe buffer

**Example:**

.. code-block:: c

   int pipefd[2];
   char buffer[256];
   
   if (pipe(pipefd) < 0) {
       // Error creating pipe
       exit(1);
   }
   
   pid_t pid = fork();
   if (pid == 0) {
       // Child process
       close(pipefd[1]);  // Close write end
       read(pipefd[0], buffer, sizeof(buffer));
       close(pipefd[0]);
   } else {
       // Parent process
       close(pipefd[0]);  // Close read end
       write(pipefd[1], "Hello child!", 12);
       close(pipefd[1]);
   }

Pipe Behavior
-------------

Blocking Semantics
~~~~~~~~~~~~~~~~~~

ThunderOS pipes currently use **non-blocking** semantics:

- **Reading from empty pipe with write end open**: Returns ``-EAGAIN`` (caller should retry)
- **Reading from empty pipe with write end closed**: Returns ``0`` (EOF)
- **Writing to full pipe**: Returns ``-EAGAIN`` (caller should retry)
- **Writing to pipe with read end closed**: Returns ``-EPIPE`` (broken pipe)

.. note::
   Future versions may support blocking I/O with process scheduling integration.

Circular Buffer
~~~~~~~~~~~~~~~

The pipe buffer is managed as a circular queue:

1. Data is written at ``write_pos`` and wraps at ``PIPE_BUF_SIZE``
2. Data is read from ``read_pos`` and wraps at ``PIPE_BUF_SIZE``
3. ``data_size`` tracks the number of bytes currently in the buffer
4. Available space = ``PIPE_BUF_SIZE - data_size``

Reference Counting
~~~~~~~~~~~~~~~~~~

Pipes use reference counting to track open ends:

- ``read_ref_count``: Number of open read file descriptors
- ``write_ref_count``: Number of open write file descriptors

When a file descriptor is closed:

1. The appropriate reference count is decremented
2. If count reaches 0, that end is marked closed
3. If both ends are closed (``state == PIPE_CLOSED``), the pipe is freed

Implementation Details
----------------------

Core Functions
~~~~~~~~~~~~~~

.. code-block:: c

   pipe_t* pipe_create(void);
   int pipe_read(pipe_t* pipe, void* buffer, size_t count);
   int pipe_write(pipe_t* pipe, const void* buffer, size_t count);
   int pipe_close_read(pipe_t* pipe);
   int pipe_close_write(pipe_t* pipe);
   void pipe_free(pipe_t* pipe);

**pipe_create()**: Allocates a new pipe with both ends open (ref counts = 1).

**pipe_read()**: Reads up to ``count`` bytes from the pipe into ``buffer``. Handles circular buffer wraparound automatically.

**pipe_write()**: Writes up to ``count`` bytes from ``buffer`` into the pipe. Returns ``-EAGAIN`` if pipe is full.

**pipe_close_read()**: Decrements read reference count and updates state.

**pipe_close_write()**: Decrements write reference count and updates state.

**pipe_free()**: Deallocates pipe structure (should only be called when both ends closed).

Memory Management
~~~~~~~~~~~~~~~~~

- Pipes are allocated with ``kmalloc(sizeof(pipe_t))`` (~4KB)
- The 4KB buffer is embedded in the ``pipe_t`` structure (no separate allocation)
- Pipes are freed with ``kfree()`` when both ends are closed

Error Handling
~~~~~~~~~~~~~~

All pipe functions follow ThunderOS errno conventions:

- On success: Call ``clear_errno()`` and return success value
- On error: Call ``set_errno(error_code)`` and return ``-1``
- Syscall wrapper checks errno and returns ``SYSCALL_ERROR`` on failure

Common error codes:

- ``THUNDEROS_EINVAL``: Invalid pointer or arguments
- ``THUNDEROS_ENOMEM``: Memory allocation failed
- ``THUNDEROS_EAGAIN``: Operation would block (pipe empty/full)
- ``THUNDEROS_EPIPE``: Broken pipe (other end closed)

Use Cases
---------

Parent-Child Communication
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   int pipefd[2];
   pipe(pipefd);
   
   if (fork() == 0) {
       // Child: reads from parent
       close(pipefd[1]);
       char msg[64];
       int n = read(pipefd[0], msg, sizeof(msg));
       // Process message...
       close(pipefd[0]);
   } else {
       // Parent: writes to child
       close(pipefd[0]);
       write(pipefd[1], "Hello child", 11);
       close(pipefd[1]);
       wait(NULL);
   }

Pipeline Execution
~~~~~~~~~~~~~~~~~~

Future shell implementation will use pipes for command pipelines:

.. code-block:: bash

   cat file.txt | grep "pattern" | wc -l

This creates two pipes connecting three processes:

1. ``cat`` writes to pipe1
2. ``grep`` reads from pipe1, writes to pipe2
3. ``wc`` reads from pipe2, writes to stdout

Limitations
-----------

Current Limitations
~~~~~~~~~~~~~~~~~~~

1. **No blocking I/O**: Pipes return ``-EAGAIN`` instead of blocking. Userspace must implement retry loops.

2. **No atomic writes > PIPE_BUF**: POSIX guarantees atomic writes up to ``PIPE_BUF`` bytes. ThunderOS may split writes if buffer space is limited.

3. **No ``pipe2()`` syscall**: Cannot set ``O_NONBLOCK`` or ``O_CLOEXEC`` flags at creation time.

4. **Fixed buffer size**: All pipes use 4KB buffers, cannot be configured.

5. **Global file descriptor table**: Pipes are currently in the global FD table. Per-process FD tables would improve isolation.

Future Enhancements
~~~~~~~~~~~~~~~~~~~

Planned improvements for v0.6.0:

- **Blocking I/O**: Integrate with scheduler to block reader/writer when pipe empty/full
- **Atomic writes**: Guarantee atomicity for writes ≤ ``PIPE_BUF`` (4096 bytes)
- **Configurable buffer size**: Allow larger pipes for high-throughput applications
- **Pipe statistics**: Track bytes transferred, max buffer usage, etc.
- **Named pipes (FIFOs)**: Filesystem-backed pipes for unrelated process communication

Testing
-------

The ``pipe_test`` userland program validates pipe functionality:

.. code-block:: bash

   # Build and run
   make userland
   make qemu
   > exec /pipe_test

**Test Cases:**

1. **Pipe creation**: Verify ``pipe()`` returns two valid file descriptors
2. **Parent-child communication**: Fork, parent writes message, child reads and validates
3. **Data integrity**: Ensure message content matches exactly
4. **Reference counting**: Verify pipe is freed when both ends closed

Expected output::

   === Pipe Test Program ===
   
   [TEST 1] Creating pipe...
   [PASS] Pipe created successfully
     Read FD:  3
     Write FD: 4
   
   [TEST 2] Forking child process...
   [PARENT] Parent process (child PID = 2)
   [PARENT] Closing read end of pipe...
   [PARENT] Writing to pipe: "Hello from parent through pipe!"
   [PARENT] Wrote 32 bytes
   [PARENT] Closing write end...
   [PARENT] Waiting for child to exit...
   [CHILD] Child process started
   [CHILD] Closing write end of pipe...
   [CHILD] Reading from pipe...
   [CHILD] Read 32 bytes: "Hello from parent through pipe!"
   [CHILD] [PASS] Message matches!
   [CHILD] Closing read end...
   [CHILD] Exiting...
   [PARENT] Child exited
   
   [PASS] All pipe tests completed successfully!

See Also
--------

- :doc:`vfs` - Virtual Filesystem integration
- :doc:`syscalls` - System call interface
- :doc:`process` - Process management and fork()
- :doc:`errno` - Error handling conventions
