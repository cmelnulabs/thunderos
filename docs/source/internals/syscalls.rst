System Calls
=============

Overview
--------

System calls (syscalls) provide the interface between user-space programs and the kernel. They allow unprivileged processes to request services from the operating system in a controlled manner.

**Key Features:**

* **Privilege separation**: User programs run in U-mode, kernel runs in S-mode
* **Controlled interface**: Only predefined operations allowed
* **Parameter validation**: Kernel validates all user-provided pointers and values
* **RISC-V ECALL**: Uses ``ecall`` instruction to trap into supervisor mode

**Call Convention:**

* **Syscall number**: Passed in register ``a7`` (x17)
* **Arguments**: Up to 6 arguments in registers ``a0-a5`` (x10-x15)
* **Return value**: Result returned in register ``a0`` (x10)
* **Error indication**: ``-1`` (all bits set) indicates error

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

Returns ``-1`` (not implemented - requires per-process heap management).

Future Syscalls
---------------

The following syscalls are defined but not yet implemented:

sys_fork (7)
~~~~~~~~~~~~

Create child process (copy of current process).

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

Performance Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Context switch overhead**: Each syscall requires S-mode ↔ U-mode transition
* **Register save/restore**: All 32 registers saved on trap entry
* **Parameter validation**: Pointer checks add overhead but are essential for security

**Typical syscall latency on QEMU**: ~1000-2000 cycles

See Also
--------

* :doc:`trap_handler` - Trap handling mechanism
* :doc:`process` - Process management
* :doc:`../riscv/interrupts_exceptions` - RISC-V trap architecture
