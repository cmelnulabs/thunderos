System Calls
=============

Overview
--------

System calls (syscalls) provide the interface between user-space programs and the kernel. They allow unprivileged processes to request services from the operating system in a controlled manner.

**Current Status:**

ThunderOS v0.4.0 implements **25 system calls** covering process management, I/O, filesystem operations, signals, memory management, and inter-process communication.

**Key Features:**

* **Privilege separation**: User programs run in U-mode, kernel runs in S-mode
* **Controlled interface**: Only predefined operations allowed
* **Parameter validation**: Kernel validates all user-provided pointers and values
* **RISC-V ECALL**: Uses ``ecall`` instruction to trap into supervisor mode

**Call Convention:**

* **Syscall number**: Passed in register ``a7`` (x17)
* **Arguments**: Up to 6 arguments in registers ``a0-a5`` (x10-x15)
* **Return value**: Result returned in register ``a0`` (x10)
* **Error indication**: ``-1`` (all bits set) indicates error, check ``errno`` for details

**System Call Limits:**

.. code-block:: c

    #define SYSCALL_MAX_PATH  4096  /* Maximum path length (4KB) */
    #define SYSCALL_MAX_ARGC  256   /* Maximum argument count for execve */

These limits prevent buffer overflow attacks and excessive resource consumption during syscall parameter validation:

- **SYSCALL_MAX_PATH**: Maximum length for file paths in ``open``, ``stat``, ``execve``, etc.
  Paths longer than 4KB are rejected with ``THUNDEROS_EINVAL``. This prevents:
  
  * Stack overflow from unbounded path copying
  * Kernel memory exhaustion
  * Denial of service attacks
  
- **SYSCALL_MAX_ARGC**: Maximum argument count for ``execve``. Prevents:
  
  * Kernel stack overflow from excessive arguments
  * Memory exhaustion from argument vector allocation
  * Process creation denial of service

**Example Validation:**

.. code-block:: c

    // Path validation in sys_open()
    if (strnlen(path, SYSCALL_MAX_PATH) >= SYSCALL_MAX_PATH) {
        RETURN_ERRNO(THUNDEROS_EINVAL);  // Path too long
    }
    
    // Argument count validation in sys_execve()
    if (argc > SYSCALL_MAX_ARGC) {
        RETURN_ERRNO(THUNDEROS_EINVAL);  // Too many arguments
    }

Architecture
------------

Syscall Flow
~~~~~~~~~~~~

.. code-block:: text

   User Program
        │
        ├─ Load syscall number into a7
        ├─ Load arguments into a0-a5
        ├─ Execute ECALL instruction
        │
        v
   ┌─────────────────────────────────────┐
   │ HARDWARE: RISC-V CPU                │
   │ - Save PC to sepc                   │
   │ - Switch to S-mode                  │
   │ - Disable interrupts                │
   │ - Jump to trap vector (stvec)       │
   └─────────────────────────────────────┘
        │
        v
   ┌─────────────────────────────────────┐
   │ ASSEMBLY: trap_vector (trap_entry.S)│
   │ - Save all registers to trap frame  │
   │ - Call trap_handler()                │
   └─────────────────────────────────────┘
        │
        v
   ┌─────────────────────────────────────┐
   │ C: trap_handler() (trap.c)          │
   │ - Check if cause == ECALL           │
   │ - Call syscall_handler()             │
   └─────────────────────────────────────┘
        │
        v
   ┌─────────────────────────────────────┐
   │ C: syscall_handler() (syscall.c)    │
   │ - Read a7 (syscall number)          │
   │ - Switch on syscall number          │
   │ - Call appropriate sys_*() function │
   │ - Return result in a0               │
   └─────────────────────────────────────┘
        │
        v
   ┌─────────────────────────────────────┐
   │ C: handle_exception() (trap.c)      │
   │ - Store return value in tf->a0      │
   │ - Advance sepc past ECALL (+4)      │
   └─────────────────────────────────────┘
        │
        v
   ┌─────────────────────────────────────┐
   │ ASSEMBLY: trap_vector restore       │
   │ - Restore all registers             │
   │ - Execute SRET instruction          │
   └─────────────────────────────────────┘
        │
        v
   User Program Continues
   (return value in a0)

Parameter Validation
~~~~~~~~~~~~~~~~~~~~

All syscalls validate user-provided pointers before dereferencing:

.. code-block:: c

   static int is_valid_user_pointer(const void *pointer, size_t length) {
       uintptr_t address = (uintptr_t)pointer;
       
       // Check for NULL
       if (pointer == NULL) {
           return 0;
       }
       
       // User addresses must be in lower half (< 0x8000000000000000)
       if (address >= KERNEL_SPACE_START) {
           return 0;
       }
       
       // Check for overflow
       if (address + length < address) {
           return 0;
       }
       
       return 1;
   }

**Why this matters:**

* Prevents user programs from reading/writing kernel memory
* Detects NULL pointer dereferences
* Catches integer overflows in buffer sizes

Available Syscalls
------------------

Process Management
~~~~~~~~~~~~~~~~~~

sys_exit (0)
^^^^^^^^^^^^

Terminate the current process.

.. code-block:: c

   void sys_exit(int status);

**Parameters:**

* ``status``: Exit status code (0 = success, non-zero = error)

**Return Value:**

* Never returns

**Example:**

.. code-block:: c

   // User-space code
   #define SYS_EXIT 0
   
   void exit(int status) {
       register long a7 asm("a7") = SYS_EXIT;
       register long a0 asm("a0") = status;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   }
   
   int main(void) {
       // ... do work ...
       exit(0);  // Terminate with success
   }

**Implementation:**

Calls ``process_exit(status)`` which:

1. Marks process as ZOMBIE
2. Releases resources
3. Never returns (scheduler picks next process)

sys_getpid (3)
^^^^^^^^^^^^^^

Get the current process ID.

.. code-block:: c

   int sys_getpid(void);

**Parameters:**

* None

**Return Value:**

* Process ID (PID) on success
* ``-1`` on error (should never happen)

**Example:**

.. code-block:: c

   #define SYS_GETPID 3
   
   int getpid(void) {
       register long a7 asm("a7") = SYS_GETPID;
       register long a0 asm("a0");
       asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   int main(void) {
       int pid = getpid();
       // Use pid...
   }

**Implementation:**

Returns ``process_current()->pid``.

sys_getppid (10)
^^^^^^^^^^^^^^^^

Get the parent process ID.

.. code-block:: c

   int sys_getppid(void);

**Parameters:**

* None

**Return Value:**

* Parent process ID (PPID)
* ``0`` if no parent (init process)

**Example:**

.. code-block:: c

   #define SYS_GETPPID 10
   
   int getppid(void) {
       register long a7 asm("a7") = SYS_GETPPID;
       register long a0 asm("a0");
       asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
       return a0;
   }

**Current Limitation:**

Returns 0 (not implemented yet - requires adding ``ppid`` field to PCB).

sys_yield (6)
^^^^^^^^^^^^^

Yield CPU to the scheduler.

.. code-block:: c

   int sys_yield(void);

**Parameters:**

* None

**Return Value:**

* ``0`` on success

**Example:**

.. code-block:: c

   #define SYS_YIELD 6
   
   int yield(void) {
       register long a7 asm("a7") = SYS_YIELD;
       register long a0 asm("a0");
       asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   void busy_wait_cooperative(void) {
       while (condition_not_met()) {
           yield();  // Let other processes run
       }
   }

**Implementation:**

Calls ``process_yield()`` which invokes the scheduler to pick the next process.

sys_sleep (5)
^^^^^^^^^^^^^

Sleep for specified milliseconds.

.. code-block:: c

   int sys_sleep(uint64_t milliseconds);

**Parameters:**

* ``milliseconds``: Time to sleep in milliseconds

**Return Value:**

* ``0`` on success

**Example:**

.. code-block:: c

   #define SYS_SLEEP 5
   
   int sleep_ms(uint64_t ms) {
       register long a7 asm("a7") = SYS_SLEEP;
       register long a0 asm("a0") = ms;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   int main(void) {
       sleep_ms(1000);  // Sleep for 1 second
   }

**Current Implementation:**

Just calls ``process_yield()`` once. TODO: Implement proper timer-based sleep.

sys_kill (11)
^^^^^^^^^^^^^

Send signal to process.

.. code-block:: c

   int sys_kill(int pid, int signal);

**Parameters:**

* ``pid``: Target process ID
* ``signal``: Signal number (not yet implemented)

**Return Value:**

* ``0`` on success
* ``-1`` on error (invalid PID, process not found)

**Example:**

.. code-block:: c

   #define SYS_KILL 11
   #define SIGTERM 15
   
   int kill(int pid, int signal) {
       register long a7 asm("a7") = SYS_KILL;
       register long a0 asm("a0") = pid;
       register long a1 asm("a1") = signal;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
       return a0;
   }
   
   int main(void) {
       kill(42, SIGTERM);  // Terminate process 42
   }

**Current Limitation:**

Returns ``-1`` (not fully implemented - requires process lookup and signal handling).

Input/Output
~~~~~~~~~~~~

sys_write (1)
^^^^^^^^^^^^^

Write data to a file descriptor.

.. code-block:: c

   ssize_t sys_write(int fd, const char *buf, size_t count);

**Parameters:**

* ``fd``: File descriptor (1 = stdout, 2 = stderr)
* ``buf``: Buffer containing data to write
* ``count``: Number of bytes to write

**Return Value:**

* Number of bytes written on success
* ``-1`` on error (invalid FD, bad pointer, I/O error)

**Example:**

.. code-block:: c

   #define SYS_WRITE 1
   #define STDOUT_FD 1
   
   ssize_t write(int fd, const void *buf, size_t count) {
       register long a7 asm("a7") = SYS_WRITE;
       register long a0 asm("a0") = fd;
       register long a1 asm("a1") = (long)buf;
       register long a2 asm("a2") = count;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   int main(void) {
       const char *msg = "Hello, World!\n";
       write(STDOUT_FD, msg, 14);
   }

**Implementation:**

1. Validates buffer pointer with ``is_valid_user_pointer()``
2. Checks file descriptor (only 1 and 2 supported currently)
3. Calls ``hal_uart_write(buf, count)``
4. Returns number of bytes written

sys_read (2)
^^^^^^^^^^^^

Read data from a file descriptor.

.. code-block:: c

   ssize_t sys_read(int fd, char *buf, size_t count);

**Parameters:**

* ``fd``: File descriptor (0 = stdin)
* ``buf``: Buffer to read into
* ``count``: Maximum number of bytes to read

**Return Value:**

* Number of bytes read on success
* ``0`` on EOF
* ``-1`` on error (invalid FD, bad pointer)

**Example:**

.. code-block:: c

   #define SYS_READ 2
   #define STDIN_FD 0
   
   ssize_t read(int fd, void *buf, size_t count) {
       register long a7 asm("a7") = SYS_READ;
       register long a0 asm("a0") = fd;
       register long a1 asm("a1") = (long)buf;
       register long a2 asm("a2") = count;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   int main(void) {
       char buffer[128];
       ssize_t n = read(STDIN_FD, buffer, sizeof(buffer));
       // Process n bytes...
   }

**Current Limitation:**

Always returns ``0`` (EOF). Requires input buffering implementation.

Time Management
~~~~~~~~~~~~~~~

sys_gettime (12)
^^^^^^^^^^^^^^^^

Get system time in milliseconds since boot.

.. code-block:: c

   uint64_t sys_gettime(void);

**Parameters:**

* None

**Return Value:**

* Milliseconds since system boot

**Example:**

.. code-block:: c

   #define SYS_GETTIME 12
   
   uint64_t gettime(void) {
       register long a7 asm("a7") = SYS_GETTIME;
       register long a0 asm("a0");
       asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   int main(void) {
       uint64_t start = gettime();
       // ... do work ...
       uint64_t end = gettime();
       uint64_t elapsed = end - start;
   }

**Implementation:**

1. Calls ``hal_timer_get_ticks()`` to get raw timer value
2. Converts ticks to milliseconds (assumes 10MHz clock: 10,000 ticks = 1ms)
3. Returns result

Memory Management
~~~~~~~~~~~~~~~~~

sys_sbrk (4)
^^^^^^^^^^^^

Adjust process heap size.

.. code-block:: c

   void *sys_sbrk(int increment);

**Parameters:**

* ``increment``: Bytes to add to heap (can be negative to shrink)

**Return Value:**

* Previous heap end address on success
* ``(void*)-1`` on error

**Example:**

.. code-block:: c

   #define SYS_SBRK 4
   
   void *sbrk(int increment) {
       register long a7 asm("a7") = SYS_SBRK;
       register long a0 asm("a0") = increment;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return (void*)a0;
   }
   
   // Simple malloc implementation
   void *malloc(size_t size) {
       return sbrk(size);
   }

**Current Limitation:**

The heap is dynamically managed per-process with safety margins to prevent stack/heap collisions.

Process Management
~~~~~~~~~~~~~~~~~~

sys_waitpid (9)
^^^^^^^^^^^^^^^

Wait for a child process to change state (terminate).

.. code-block:: c

   pid_t sys_waitpid(int pid, int *wstatus, int options);

**Parameters:**

* ``pid``: Process ID to wait for:
  
  * ``-1`` = wait for any child process
  * ``> 0`` = wait for specific child PID

* ``wstatus``: Pointer to store exit status (can be NULL)
* ``options``: Options (currently unused, pass 0)

**Return Value:**

* PID of terminated child on success
* ``-1`` on error (no such child, invalid PID, etc.)

**Example:**

.. code-block:: c

   #define SYS_WAIT 9
   
   pid_t waitpid(int pid, int *wstatus, int options) {
       register long a7 asm("a7") = SYS_WAIT;
       register long a0 asm("a0") = pid;
       register long a1 asm("a1") = (long)wstatus;
       register long a2 asm("a2") = options;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   int main(void) {
       int status;
       pid_t child_pid = waitpid(-1, &status, 0);  // Wait for any child
       int exit_code = (status >> 8) & 0xFF;
   }

**Implementation:**

1. Validates PID parameter
2. Searches process table for zombie children matching PID
3. If found, retrieves exit status and frees child process
4. If no zombie found but child exists, yields CPU and retries
5. Returns error if no matching child exists

Filesystem Operations
~~~~~~~~~~~~~~~~~~~~~

sys_open (13)
^^^^^^^^^^^^^

Open a file and return a file descriptor.

.. code-block:: c

   int sys_open(const char *path, int flags, int mode);

**Parameters:**

* ``path``: File path (absolute, max 4096 bytes)
* ``flags``: Open flags (``O_RDONLY``, ``O_WRONLY``, ``O_RDWR``, ``O_CREAT``, etc.)
* ``mode``: File permissions (unused currently)

**Return Value:**

* File descriptor (≥ 0) on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path or flags
* ``THUNDEROS_ENOENT`` - File not found
* ``THUNDEROS_ENOMEM`` - Out of file descriptors

**Example:**

.. code-block:: c

   #define SYS_OPEN 13
   #define O_RDONLY 0
   
   int open(const char *path, int flags, int mode) {
       register long a7 asm("a7") = SYS_OPEN;
       register long a0 asm("a0") = (long)path;
       register long a1 asm("a1") = flags;
       register long a2 asm("a2") = mode;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   int fd = open("/test.txt", O_RDONLY, 0);

**Implementation:**

1. Validates path pointer and length (max ``SYSCALL_MAX_PATH``)
2. Calls ``vfs_open()`` to resolve path and open file
3. Returns allocated file descriptor

sys_close (14)
^^^^^^^^^^^^^^

Close an open file descriptor.

.. code-block:: c

   int sys_close(int fd);

**Parameters:**

* ``fd``: File descriptor to close

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EBADF`` - Invalid file descriptor

**Example:**

.. code-block:: c

   #define SYS_CLOSE 14
   
   int close(int fd) {
       register long a7 asm("a7") = SYS_CLOSE;
       register long a0 asm("a0") = fd;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   close(fd);

sys_lseek (15)
^^^^^^^^^^^^^^

Reposition read/write file offset.

.. code-block:: c

   int64_t sys_lseek(int fd, int64_t offset, int whence);

**Parameters:**

* ``fd``: File descriptor
* ``offset``: Offset in bytes
* ``whence``: Position reference:
  
  * ``SEEK_SET`` (0) - Beginning of file
  * ``SEEK_CUR`` (1) - Current position
  * ``SEEK_END`` (2) - End of file

**Return Value:**

* New file offset on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EBADF`` - Invalid file descriptor
* ``THUNDEROS_EINVAL`` - Invalid whence value

**Example:**

.. code-block:: c

   #define SYS_LSEEK 15
   #define SEEK_SET 0
   
   int64_t lseek(int fd, int64_t offset, int whence) {
       register long a7 asm("a7") = SYS_LSEEK;
       register long a0 asm("a0") = fd;
       register long a1 asm("a1") = offset;
       register long a2 asm("a2") = whence;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   lseek(fd, 0, SEEK_SET);  // Rewind to start

sys_stat (16)
^^^^^^^^^^^^^

Get file status information.

.. code-block:: c

   int sys_stat(const char *path, struct stat *statbuf);

**Parameters:**

* ``path``: File path (absolute)
* ``statbuf``: Pointer to stat structure

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path or buffer pointer
* ``THUNDEROS_ENOENT`` - File not found

**Example:**

.. code-block:: c

   #define SYS_STAT 16
   
   struct stat {
       uint32_t st_size;   // File size in bytes
       uint32_t st_mode;   // File type and permissions
   };
   
   int stat(const char *path, struct stat *buf) {
       register long a7 asm("a7") = SYS_STAT;
       register long a0 asm("a0") = (long)path;
       register long a1 asm("a1") = (long)buf;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
       return a0;
   }
   
   struct stat st;
   if (stat("/test.txt", &st) == 0) {
       // st.st_size contains file size
   }

sys_mkdir (17)
^^^^^^^^^^^^^^

Create a new directory.

.. code-block:: c

   int sys_mkdir(const char *path, int mode);

**Parameters:**

* ``path``: Directory path
* ``mode``: Directory permissions (currently unused)

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path
* ``THUNDEROS_EEXIST`` - Directory already exists

**Example:**

.. code-block:: c

   #define SYS_MKDIR 17
   
   int mkdir(const char *path, int mode) {
       register long a7 asm("a7") = SYS_MKDIR;
       register long a0 asm("a0") = (long)path;
       register long a1 asm("a1") = mode;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
       return a0;
   }
   
   mkdir("/mydir", 0755);

sys_unlink (18)
^^^^^^^^^^^^^^^

Remove a file (delete).

.. code-block:: c

   int sys_unlink(const char *path);

**Parameters:**

* ``path``: File path to remove

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path
* ``THUNDEROS_ENOENT`` - File not found

**Example:**

.. code-block:: c

   #define SYS_UNLINK 18
   
   int unlink(const char *path) {
       register long a7 asm("a7") = SYS_UNLINK;
       register long a0 asm("a0") = (long)path;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   unlink("/temp.txt");

sys_rmdir (19)
^^^^^^^^^^^^^^

Remove an empty directory.

.. code-block:: c

   int sys_rmdir(const char *path);

**Parameters:**

* ``path``: Directory path to remove

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path
* ``THUNDEROS_ENOENT`` - Directory not found
* ``THUNDEROS_ENOTEMPTY`` - Directory not empty

**Example:**

.. code-block:: c

   #define SYS_RMDIR 19
   
   int rmdir(const char *path) {
       register long a7 asm("a7") = SYS_RMDIR;
       register long a0 asm("a0") = (long)path;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   rmdir("/mydir");

sys_execve (20)
^^^^^^^^^^^^^^^

Execute a program loaded from the filesystem.

.. code-block:: c

   int sys_execve(const char *path, const char *argv[], const char *envp[]);

**Parameters:**

* ``path``: Path to ELF executable (max ``SYSCALL_MAX_PATH`` bytes)
* ``argv``: NULL-terminated argument vector (max ``SYSCALL_MAX_ARGC`` arguments)
* ``envp``: NULL-terminated environment vector (currently unused)

**Return Value:**

* Does not return on success (replaces current process)
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid path, too many arguments
* ``THUNDEROS_ENOENT`` - File not found
* ``THUNDEROS_EELF_MAGIC`` - Not a valid ELF file

**Example:**

.. code-block:: c

   #define SYS_EXECVE 20
   
   int execve(const char *path, const char *argv[], const char *envp[]) {
       register long a7 asm("a7") = SYS_EXECVE;
       register long a0 asm("a0") = (long)path;
       register long a1 asm("a1") = (long)argv;
       register long a2 asm("a2") = (long)envp;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
       return a0;
   }
   
   const char *args[] = {"/bin/cat", "/test.txt", NULL};
   execve("/bin/cat", args, NULL);  // Does not return

**Implementation:**

1. Validates path and argument count
2. Calls ``elf_load_exec()`` to load ELF binary
3. Replaces current process memory with new program
4. Initializes stack with arguments
5. Returns to user mode at ELF entry point

Signal Handling
~~~~~~~~~~~~~~~

sys_signal (21)
^^^^^^^^^^^^^^^

Install a signal handler (simplified interface).

.. code-block:: c

   void (*sys_signal(int signum, void (*handler)(int)))(int);

**Parameters:**

* ``signum``: Signal number (``SIGHUP``, ``SIGINT``, ``SIGKILL``, etc.)
* ``handler``: Signal handler function or:
  
  * ``SIG_DFL`` - Default action
  * ``SIG_IGN`` - Ignore signal

**Return Value:**

* Previous signal handler on success
* ``SIG_ERR`` (``-1``) on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid signal number

**Example:**

.. code-block:: c

   #define SYS_SIGNAL 21
   #define SIGUSR1 10
   
   typedef void (*sighandler_t)(int);
   
   sighandler_t signal(int signum, sighandler_t handler) {
       register long a7 asm("a7") = SYS_SIGNAL;
       register long a0 asm("a0") = signum;
       register long a1 asm("a1") = (long)handler;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
       return (sighandler_t)a0;
   }
   
   void sigusr1_handler(int sig) {
       // Handle signal
   }
   
   signal(SIGUSR1, sigusr1_handler);

sys_sigaction (22)
^^^^^^^^^^^^^^^^^^

Advanced signal handling with signal masks and flags.

.. code-block:: c

   int sys_sigaction(int signum, const struct sigaction *act, 
                     struct sigaction *oldact);

**Parameters:**

* ``signum``: Signal number
* ``act``: New signal action (can be NULL to query current)
* ``oldact``: Previous signal action (can be NULL)

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid signal number or action

**Note:** Currently not fully implemented. Use ``sys_signal()`` instead.

sys_sigreturn (23)
^^^^^^^^^^^^^^^^^^

Return from a signal handler (restore context).

.. code-block:: c

   int sys_sigreturn(void);

**Parameters:**

* None (context restored from trap frame)

**Return Value:**

* Does not return normally (restores user context)

**Note:** This syscall is called automatically by the signal trampoline. User code should not call it directly.

Memory Management
~~~~~~~~~~~~~~~~~

sys_mmap (24)
^^^^^^^^^^^^^

Map memory into the process address space.

.. code-block:: c

   void *sys_mmap(void *addr, size_t length, int prot, int flags, 
                  int fd, uint64_t offset);

**Parameters:**

* ``addr``: Requested address (NULL for kernel to choose)
* ``length``: Length of mapping in bytes
* ``prot``: Memory protection:
  
  * ``PROT_READ`` (0x1) - Readable
  * ``PROT_WRITE`` (0x2) - Writable
  * ``PROT_EXEC`` (0x4) - Executable

* ``flags``: Mapping flags:
  
  * ``MAP_PRIVATE`` (0x02) - Private copy-on-write
  * ``MAP_ANONYMOUS`` (0x20) - Not backed by file

* ``fd``: File descriptor (only for file-backed mappings)
* ``offset``: Offset in file

**Return Value:**

* Mapped address on success
* ``MAP_FAILED`` (``-1``) on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid parameters
* ``THUNDEROS_ENOMEM`` - Out of memory

**Example:**

.. code-block:: c

   #define SYS_MMAP 24
   #define PROT_READ  0x1
   #define PROT_WRITE 0x2
   #define MAP_PRIVATE 0x02
   #define MAP_ANONYMOUS 0x20
   #define MAP_FAILED ((void*)-1)
   
   void *mmap(void *addr, size_t length, int prot, int flags, 
              int fd, uint64_t offset) {
       register long a7 asm("a7") = SYS_MMAP;
       register long a0 asm("a0") = (long)addr;
       register long a1 asm("a1") = length;
       register long a2 asm("a2") = prot;
       register long a3 asm("a3") = flags;
       register long a4 asm("a4") = fd;
       register long a5 asm("a5") = offset;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2), 
                    "r"(a3), "r"(a4), "r"(a5) : "memory");
       return (void*)a0;
   }
   
   // Allocate anonymous memory
   void *buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

**Implementation:**

1. Validates parameters (alignment, size, flags)
2. Allocates physical pages
3. Creates page table mappings with requested permissions
4. Adds VMA to process for memory isolation tracking
5. Returns mapped virtual address

sys_munmap (25)
^^^^^^^^^^^^^^^

Unmap a previously mapped memory region.

.. code-block:: c

   int sys_munmap(void *addr, size_t length);

**Parameters:**

* ``addr``: Address of mapping to unmap
* ``length``: Length of region

**Return Value:**

* ``0`` on success
* ``-1`` on error

**Errno:**

* ``THUNDEROS_EINVAL`` - Invalid address or length

**Example:**

.. code-block:: c

   #define SYS_MUNMAP 25
   
   int munmap(void *addr, size_t length) {
       register long a7 asm("a7") = SYS_MUNMAP;
       register long a0 asm("a0") = (long)addr;
       register long a1 asm("a1") = length;
       asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1) : "memory");
       return a0;
   }
   
   munmap(buffer, 4096);

**Implementation:**

1. Validates address and length
2. Removes page table mappings
3. Frees physical pages
4. Removes VMA from process
5. Flushes TLB

sys_pipe (26)
~~~~~~~~~~~~~

**Prototype:**

.. code-block:: c

   int sys_pipe(int pipefd[2]);

**Description:**

Creates an anonymous pipe for inter-process communication. The pipe provides unidirectional data flow with a 4KB circular buffer.

Returns two file descriptors:

- ``pipefd[0]``: Read end of the pipe
- ``pipefd[1]``: Write end of the pipe

Data written to the write end can be read from the read end. Pipes are typically used after ``fork()`` to enable parent-child communication or to implement shell pipelines.

**Parameters:**

- ``pipefd[2]``: Array to receive the two file descriptors

**Return Value:**

- ``0`` on success
- ``-1`` on error (check ``errno``)

**Error Codes:**

.. code-block:: c

   THUNDEROS_EINVAL  // Invalid pipefd pointer
   THUNDEROS_EMFILE  // Too many open files (FD table full)
   THUNDEROS_ENOMEM  // Failed to allocate pipe buffer

**Example:**

.. code-block:: c

   #define SYS_PIPE 26
   
   int pipe(int pipefd[2]) {
       register long a0 asm("a0") = (long)pipefd;
       register long a7 asm("a7") = SYS_PIPE;
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
       return a0;
   }
   
   // Parent-child communication
   int pipefd[2];
   pipe(pipefd);
   
   if (fork() == 0) {
       // Child: reads from parent
       close(pipefd[1]);  // Close write end
       char buf[256];
       read(pipefd[0], buf, sizeof(buf));
       close(pipefd[0]);
   } else {
       // Parent: writes to child
       close(pipefd[0]);  // Close read end
       write(pipefd[1], "Hello child!", 12);
       close(pipefd[1]);
   }

**Implementation:**

1. Validates user pointer against process VMAs (writable)
2. Allocates pipe structure (4KB circular buffer)
3. Allocates two file descriptors from VFS
4. Initializes read end (``pipefd[0]``, ``O_RDONLY``)
5. Initializes write end (``pipefd[1]``, ``O_WRONLY``)
6. Returns file descriptors to userspace

**Pipe Behavior:**

- **Reading from empty pipe (write end open)**: Returns ``-EAGAIN`` (non-blocking)
- **Reading from empty pipe (write end closed)**: Returns ``0`` (EOF)
- **Writing to full pipe**: Returns ``-EAGAIN`` (non-blocking)
- **Writing with read end closed**: Returns ``-EPIPE`` (broken pipe)

**Buffer Size:**

.. code-block:: c

   #define PIPE_BUF_SIZE 4096  // 4KB circular buffer

**See Also:**

- :doc:`pipes` - Complete pipe implementation documentation
- :doc:`vfs` - Virtual Filesystem integration

Future Syscalls
---------------

The following syscalls are defined but not yet fully implemented:

sys_fork (7)
~~~~~~~~~~~~

Create child process (copy of current process).

**Status:** Defined but not exposed to userland (kernel uses ``process_fork()`` internally).

**Planned Implementation:**

1. Allocate new PCB
2. Copy parent's page tables
3. Copy parent's registers
4. Return 0 in child, child PID in parent

sys_exec (8)
~~~~~~~~~~~~

Replace current process with new program.

**Planned Implementation:**

1. Load ELF binary from filesystem
2. Create new page tables for program
3. Initialize stack with arguments
4. Set PC to entry point
5. Execute program

sys_wait (9)
~~~~~~~~~~~~

Wait for child process to terminate.

**Planned Implementation:**

1. Block parent process
2. When child exits, wake parent
3. Return child's exit status

Error Codes
-----------

Syscalls return ``-1`` (``0xFFFFFFFFFFFFFFFF``) to indicate errors:

.. code-block:: c

   #define SYSCALL_ERROR ((uint64_t)-1)
   #define SYSCALL_SUCCESS 0

**Common Error Conditions:**

* **Invalid pointer**: User-provided pointer is NULL, in kernel space, or causes overflow
* **Invalid file descriptor**: FD not in range [0, 2]
* **Invalid PID**: Process ID ≤ 0 or process not found
* **Not implemented**: Syscall is defined but functionality not yet available

**Error Handling Example:**

.. code-block:: c

   ssize_t result = write(STDOUT_FD, buffer, size);
   if (result == -1) {
       // Error occurred
       exit(1);
   }

Usage Example
-------------

Complete User Program
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // user_hello.c - Simple user-space program
   
   #define SYS_WRITE  1
   #define SYS_GETPID 3
   #define SYS_EXIT   0
   #define STDOUT_FD  1
   
   // Syscall wrappers
   static inline long syscall1(long n, long a0) {
       register long a7 asm("a7") = n;
       register long ret asm("a0") = a0;
       asm volatile("ecall" : "+r"(ret) : "r"(a7) : "memory");
       return ret;
   }
   
   static inline long syscall3(long n, long a0, long a1, long a2) {
       register long a7 asm("a7") = n;
       register long ret asm("a0") = a0;
       register long _a1 asm("a1") = a1;
       register long _a2 asm("a2") = a2;
       asm volatile("ecall" : "+r"(ret) : "r"(a7), "r"(_a1), "r"(_a2) : "memory");
       return ret;
   }
   
   void exit(int status) {
       syscall1(SYS_EXIT, status);
       __builtin_unreachable();
   }
   
   int getpid(void) {
       return syscall1(SYS_GETPID, 0);
   }
   
   ssize_t write(int fd, const void *buf, size_t count) {
       return syscall3(SYS_WRITE, fd, (long)buf, count);
   }
   
   // Simple string length
   size_t strlen(const char *s) {
       size_t len = 0;
       while (s[len]) len++;
       return len;
   }
   
   // Print string
   void puts(const char *s) {
       write(STDOUT_FD, s, strlen(s));
   }
   
   // Entry point
   int main(void) {
       int pid = getpid();
       
       puts("Hello from user space!\n");
       puts("My PID is: ");
       
       // Print PID (simple decimal conversion)
       char buf[16];
       int i = 0;
       int temp = pid;
       if (temp == 0) {
           buf[i++] = '0';
       } else {
           while (temp > 0) {
               buf[i++] = '0' + (temp % 10);
               temp /= 10;
           }
       }
       // Reverse digits
       for (int j = 0; j < i/2; j++) {
           char t = buf[j];
           buf[j] = buf[i-1-j];
           buf[i-1-j] = t;
       }
       buf[i] = '\n';
       write(STDOUT_FD, buf, i+1);
       
       exit(0);
   }

Testing
-------

To test syscalls:

1. **Compile user program** (requires cross-compiler with user-space flags)
2. **Load into process** (currently processes are kernel threads)
3. **Execute and verify output**

Future: User-mode support will enable true testing of privilege separation.

Implementation Details
----------------------

Files
~~~~~

* ``kernel/core/syscall.c`` - Syscall implementations
* ``include/kernel/syscall.h`` - Syscall numbers and prototypes
* ``kernel/arch/riscv64/core/trap.c`` - Syscall trap handling

Syscall Dispatch
~~~~~~~~~~~~~~~~~

.. code-block:: c

   uint64_t syscall_handler(uint64_t syscall_num, 
                           uint64_t arg0, uint64_t arg1, uint64_t arg2,
                           uint64_t arg3, uint64_t arg4, uint64_t arg5) {
       switch (syscall_num) {
           case SYS_EXIT:
               return sys_exit((int)arg0);
           case SYS_WRITE:
               return sys_write((int)arg0, (const char*)arg1, (size_t)arg2);
           // ... more syscalls ...
           default:
               return SYSCALL_ERROR;
       }
   }

See Also
--------

* :doc:`trap_handler` - Trap handling mechanism
* :doc:`process` - Process management
* :doc:`../riscv/interrupts_exceptions` - RISC-V trap architecture
