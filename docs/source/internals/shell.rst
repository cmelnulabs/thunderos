.. _internals-shell:

User-Mode Shell (ush)
=====================

ThunderOS includes a user-mode interactive shell (``ush``) that runs entirely in user space, providing a command-line interface for system interaction.

Overview
--------

The user-mode shell (``ush`` v0.8.0) provides:

- **User-space execution**: Runs as a regular user process (PID 1)
- **Fork+exec model**: Launches external programs via fork and exec
- **Built-in commands**: Directory navigation, file operations
- **External commands**: Executes ELF binaries from filesystem

Shell Location
--------------

.. code-block:: text

    /bin/ush          - Main shell binary (ELF executable)

The kernel loads and executes ``/bin/ush`` at boot to provide the interactive environment.

Architecture
------------

Execution Model
~~~~~~~~~~~~~~~

.. code-block:: text

    ┌──────────────────────────────────────────────────────────┐
    │                    User-Mode Shell (ush)                  │
    │                                                           │
    │  ┌─────────────────────────────────────────────────────┐ │
    │  │                   Main Loop                          │ │
    │  │                                                      │ │
    │  │  1. Display prompt (ush>)                            │ │
    │  │  2. Read character from stdin                        │ │
    │  │  3. Echo character to stdout                         │ │
    │  │  4. On newline:                                      │ │
    │  │     a. Parse command                                 │ │
    │  │     b. Execute built-in OR fork+exec                 │ │
    │  │  5. Return to step 1                                 │ │
    │  └─────────────────────────────────────────────────────┘ │
    │                                                           │
    │  ┌─────────────────┐    ┌───────────────────────────────┐│
    │  │  Built-in Cmds  │    │     External Commands         ││
    │  │                 │    │                               ││
    │  │  cd, pwd, echo  │    │  fork() → child process       ││
    │  │  mkdir, rmdir   │    │  execve("/bin/program")       ││
    │  │  clear, help    │    │  waitpid() for completion     ││
    │  │  exit           │    │                               ││
    │  └─────────────────┘    └───────────────────────────────┘│
    └──────────────────────────────────────────────────────────┘

Built-in Commands
-----------------

Built-in commands execute within the shell process (no fork):

.. list-table:: Shell Built-in Commands
   :header-rows: 1
   :widths: 15 40 45

   * - Command
     - Description
     - Example
   * - ``help``
     - Display available commands
     - ``help``
   * - ``echo``
     - Echo text to stdout
     - ``echo Hello World``
   * - ``cd``
     - Change working directory
     - ``cd /bin``
   * - ``pwd``
     - Print working directory (calls external)
     - ``pwd``
   * - ``mkdir``
     - Create directory
     - ``mkdir /mydir``
   * - ``rmdir``
     - Remove empty directory
     - ``rmdir /mydir``
   * - ``clear``
     - Clear terminal screen
     - ``clear``
   * - ``exit``
     - Exit shell (terminates kernel)
     - ``exit``

External Commands
-----------------

External commands are executed via fork+exec:

.. list-table:: External Commands
   :header-rows: 1
   :widths: 15 40 45

   * - Command
     - Binary Path
     - Description
   * - ``ls``
     - ``/bin/ls``
     - List directory contents
   * - ``cat``
     - ``/bin/cat <file>``
     - Display file contents
   * - ``hello``
     - ``/bin/hello``
     - Hello world test program
   * - ``pwd``
     - ``/bin/pwd``
     - Print working directory

Fork+Exec Pattern
~~~~~~~~~~~~~~~~~

.. code-block:: c

    // External command execution (simplified)
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        const char *path = "/bin/ls";
        const char *argv[] = { path, NULL };
        const char *envp[] = { NULL };
        execve(path, argv, envp);
        exit(1);  // Only reached if exec fails
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);  // Wait for child
    }

System Calls Used
-----------------

The shell uses the following system calls:

.. code-block:: c

    // I/O
    SYS_WRITE (1)    - Write to stdout
    SYS_READ (2)     - Read from stdin
    SYS_OPEN (13)    - Open files
    SYS_CLOSE (14)   - Close file descriptors
    
    // Process
    SYS_EXIT (0)     - Exit shell
    SYS_FORK (7)     - Create child process
    SYS_WAIT (9)     - Wait for child to exit
    SYS_YIELD (6)    - Yield CPU when waiting for input
    SYS_EXECVE (20)  - Execute program
    
    // Filesystem
    SYS_MKDIR (17)   - Create directory
    SYS_RMDIR (19)   - Remove directory
    SYS_CHDIR (28)   - Change directory
    SYS_GETCWD (29)  - Get working directory

Implementation Details
----------------------

Source File
~~~~~~~~~~~

The shell is implemented in ``userland/ush_flat.c`` using a "flat" coding style optimized for ``-O0`` compilation:

- All logic in ``_start()`` function to avoid stack frame issues
- No function calls (syscall wrappers are external assembly)
- Inline string operations
- Static string constants

Global Pointer Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    void _start(void) {
        // Initialize gp register for global data access
        __asm__ volatile (
            ".option push\n"
            ".option norelax\n"
            "1: auipc gp, %%pcrel_hi(__global_pointer$)\n"
            "   addi gp, gp, %%pcrel_lo(1b)\n"
            ".option pop\n"
            ::: "gp"
        );
        // ...
    }

Input Processing
~~~~~~~~~~~~~~~~

.. code-block:: c

    // Main input loop
    while (1) {
        char c;
        long n = syscall3(SYS_READ, 0, (long)&c, 1);
        
        if (n != 1) {
            syscall0(SYS_YIELD);  // Wait for input
            continue;
        }
        
        // Echo character (handle newlines specially)
        if (c == '\r' || c == '\n') {
            syscall3(SYS_WRITE, 1, "\r\n", 2);
            // Process command...
        } else {
            syscall3(SYS_WRITE, 1, &c, 1);
            input[pos++] = c;
        }
    }

Working Directory
-----------------

The shell maintains a per-process working directory:

- Initialized to ``/`` at process creation
- Changed via ``cd`` command (calls ``sys_chdir``)
- Queried via ``pwd`` command (calls ``sys_getcwd``)
- Inherited by child processes via ``fork()``

.. code-block:: c

    // cd command implementation
    long result = syscall1(SYS_CHDIR, (long)path);
    if (result != 0) {
        print("cd: cannot access directory\n");
    }

Userland Utilities
------------------

ThunderOS includes several userland utility programs:

.. list-table:: Userland Utilities
   :header-rows: 1
   :widths: 15 25 60

   * - Utility
     - Binary
     - Description
   * - ``ls``
     - ``/bin/ls``
     - Lists directory contents using ``getdents`` syscall
   * - ``cat``
     - ``/bin/cat``
     - Displays file contents (requires argument from shell)
   * - ``pwd``
     - ``/bin/pwd``
     - Prints current working directory using ``getcwd``
   * - ``mkdir``
     - ``/bin/mkdir``
     - Creates directories (shell built-in preferred)
   * - ``rmdir``
     - ``/bin/rmdir``
     - Removes empty directories (shell built-in preferred)
   * - ``touch``
     - ``/bin/touch``
     - Creates empty files
   * - ``rm``
     - ``/bin/rm``
     - Removes files using ``unlink`` syscall
   * - ``clear``
     - ``/bin/clear``
     - Clears terminal using ANSI escape codes
   * - ``sleep``
     - ``/bin/sleep``
     - Sleeps for specified seconds
   * - ``hello``
     - ``/bin/hello``
     - Simple test program, prints greeting

Building Userland
-----------------

Userland programs are built with the RISC-V cross-compiler:

.. code-block:: bash

    # Build all userland programs
    make userland
    
    # Individual program compilation
    riscv64-unknown-elf-gcc -O0 -g -nostdlib -nostartfiles \
        -T user.ld -o build/hello userland/hello.c userland/syscall.S

Linker Script
~~~~~~~~~~~~~

Userland programs use ``userland/user.ld``:

.. code-block:: text

    ENTRY(_start)
    
    SECTIONS {
        . = 0x10000;           /* User code base */
        .text : { *(.text*) }
        .rodata : { *(.rodata*) }
        .data : { *(.data*) }
        .bss : { *(.bss*) }
    }

Known Limitations
-----------------

Current limitations of the shell and userland:

1. **Relative Paths**: Only absolute paths supported (no ``cd ..``, ``cd subdir``)
2. **No Argument Passing**: External programs don't receive command-line arguments
3. **No Pipes**: Shell syntax pipes (``cmd1 | cmd2``) not implemented
4. **No I/O Redirection**: No ``>``, ``<``, ``>>`` operators
5. **No Command History**: No up/down arrow navigation
6. **No Tab Completion**: No filename completion

Future Improvements
-------------------

Planned enhancements:

- Relative path resolution in VFS
- Command-line argument passing to programs
- Command history with arrow key navigation
- Tab completion for commands and paths
- Shell pipes and I/O redirection
- Environment variables

See Also
--------

- :doc:`syscalls` - System call reference
- :doc:`process_management` - Process creation and management
- :doc:`elf_loader` - ELF binary loading
- :doc:`vfs` - Virtual filesystem operations
