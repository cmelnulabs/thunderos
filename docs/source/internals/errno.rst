Error Handling System (errno)
==============================

ThunderOS implements a POSIX-inspired error handling system that provides semantic error codes throughout the operating system. This replaces the previous inconsistent use of magic numbers (-1, -2, -3, etc.) with meaningful, human-readable error codes.

Overview
--------

The error handling system consists of:

1. **Standardized Error Codes**: POSIX-compatible errno values organized by subsystem
2. **Per-Process errno**: Each process has its own errno variable for thread-safe error tracking
3. **Error String Conversion**: Human-readable error messages for debugging
4. **Convenience Macros**: Helper macros for consistent error handling patterns

Error Code Ranges
-----------------

Error codes are organized into ranges by subsystem:

.. code-block:: c

   /* Error Code Ranges */
   0       : Success (THUNDEROS_OK)
   1-29    : Generic POSIX-style errors
   30-49   : Filesystem errors
   50-69   : ELF loader errors
   70-89   : VirtIO/driver errors
   90-109  : Process/scheduler errors
   110-129 : Memory management errors

Common Error Codes
------------------

Generic Errors (POSIX-Compatible)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define THUNDEROS_OK          0   /* Success */
   #define THUNDEROS_EPERM       1   /* Operation not permitted */
   #define THUNDEROS_ENOENT      2   /* No such file or directory */
   #define THUNDEROS_EIO         5   /* I/O error */
   #define THUNDEROS_ENOMEM      12  /* Out of memory */
   #define THUNDEROS_EACCES      13  /* Permission denied */
   #define THUNDEROS_EBUSY       16  /* Device or resource busy */
   #define THUNDEROS_EEXIST      17  /* File exists */
   #define THUNDEROS_ENOTDIR     20  /* Not a directory */
   #define THUNDEROS_EISDIR      21  /* Is a directory */
   #define THUNDEROS_EINVAL      22  /* Invalid argument */
   #define THUNDEROS_EMFILE      24  /* Too many open files */
   #define THUNDEROS_ENOSPC      28  /* No space left on device */

Filesystem Errors
~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define THUNDEROS_EFS_CORRUPT 30  /* Filesystem corruption detected */
   #define THUNDEROS_EFS_INVAL   31  /* Invalid filesystem structure */
   #define THUNDEROS_EFS_BADBLK  32  /* Bad block number */
   #define THUNDEROS_EFS_NOINODE 33  /* No free inodes */
   #define THUNDEROS_EFS_NOBLK   34  /* No free blocks */
   #define THUNDEROS_EFS_NOTMNT  41  /* Filesystem not mounted */

ELF Loader Errors
~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define THUNDEROS_EELF_MAGIC  50  /* Invalid ELF magic number */
   #define THUNDEROS_EELF_ARCH   54  /* Wrong architecture (not RISC-V) */
   #define THUNDEROS_EELF_TYPE   55  /* Wrong ELF type (not executable) */
   #define THUNDEROS_EELF_NOPHDR 59  /* No program headers */

Process Errors
~~~~~~~~~~~~~~

.. code-block:: c

   #define THUNDEROS_EPROC_LIMIT  90  /* Process limit reached */
   #define THUNDEROS_EPROC_INIT   95  /* Process initialization failed */

Per-Process errno
-----------------

Each process has its own ``errno_value`` field in the PCB:

.. code-block:: c

   struct process {
       // ... other fields ...
       int errno_value;  /* Per-process error number */
   };

This ensures thread-safe error handling when multiple processes are running.

During early boot (before process management is initialized), a global ``g_early_errno`` is used as a fallback.

API Functions
-------------

Core Functions
~~~~~~~~~~~~~~

**__thunderos_errno_location()**

Get pointer to current process's errno variable:

.. code-block:: c

   int *__thunderos_errno_location(void);

**set_errno()**

Set errno and return -1:

.. code-block:: c

   int set_errno(int error_code);
   
   // Example:
   if (!buffer) return set_errno(THUNDEROS_ENOMEM);

**get_errno()**

Get current errno value:

.. code-block:: c

   int get_errno(void);
   
   // Example:
   int err = get_errno();
   hal_uart_puts(thunderos_strerror(err));

**clear_errno()**

Clear errno (set to THUNDEROS_OK):

.. code-block:: c

   void clear_errno(void);
   
   // Call at start of successful operations
   clear_errno();
   return 0;

Error Message Functions
~~~~~~~~~~~~~~~~~~~~~~~

**thunderos_strerror()**

Convert error code to human-readable string:

.. code-block:: c

   const char *thunderos_strerror(int error_code);
   
   // Example:
   hal_uart_puts(thunderos_strerror(errno));
   // Output: "No such file or directory"

**kernel_perror()**

Print error message to console (like POSIX perror):

.. code-block:: c

   void kernel_perror(const char *prefix);
   
   // Example:
   if (vfs_open("/foo", O_RDONLY) < 0) {
       kernel_perror("vfs_open");
       // Output: "vfs_open: No such file or directory\n"
   }

Convenience Macros
------------------

**errno**

Access current errno directly:

.. code-block:: c

   #define errno (*__thunderos_errno_location())
   
   // Usage:
   errno = THUNDEROS_EINVAL;
   if (errno == THUNDEROS_ENOMEM) { ... }

**RETURN_ERRNO(err)**

Set errno and return -1:

.. code-block:: c

   #define RETURN_ERRNO(err) return set_errno(err)
   
   // Usage:
   if (!valid) RETURN_ERRNO(THUNDEROS_EINVAL);

**RETURN_ERRNO_NULL(err)**

Set errno and return NULL:

.. code-block:: c

   #define RETURN_ERRNO_NULL(err) do { \
       set_errno(err); \
       return NULL; \
   } while(0)
   
   // Usage:
   if (!buffer) RETURN_ERRNO_NULL(THUNDEROS_ENOMEM);

**SET_ERRNO_GOTO(err, label)**

Set errno and jump to cleanup label:

.. code-block:: c

   #define SET_ERRNO_GOTO(err, label) do { \
       set_errno(err); \
       goto label; \
   } while(0)
   
   // Usage:
   if (alloc_failed) SET_ERRNO_GOTO(THUNDEROS_ENOMEM, cleanup);

Error Handling Patterns
-----------------------

Functions That Set Errors
~~~~~~~~~~~~~~~~~~~~~~~~~~

Functions should set errno for their own errors:

.. code-block:: c

   int vfs_open(const char *path, uint32_t flags) {
       if (!path) {
           RETURN_ERRNO(THUNDEROS_EINVAL);
       }
       
       vfs_node_t *node = vfs_resolve_path(path);
       if (!node) {
           RETURN_ERRNO(THUNDEROS_ENOENT);
       }
       
       int fd = vfs_alloc_fd();
       if (fd < 0) {
           /* errno already set by vfs_alloc_fd */
           return -1;
       }
       
       /* Success - clear errno */
       clear_errno();
       return fd;
   }

Error Propagation
~~~~~~~~~~~~~~~~~

Preserve errno from called functions:

.. code-block:: c

   /* Helper function - sets specific error */
   static int allocate_resource(void) {
       void *ptr = kmalloc(SIZE);
       if (!ptr) {
           RETURN_ERRNO(THUNDEROS_ENOMEM);
       }
       clear_errno();
       return 0;
   }
   
   /* Caller - preserves errno from helper */
   int high_level_operation(void) {
       if (allocate_resource() < 0) {
           /* errno already set by allocate_resource */
           return -1;
       }
       return 0;
   }

Cleanup Patterns
~~~~~~~~~~~~~~~~

Use goto for cleanup with errno:

.. code-block:: c

   int complex_operation(const char *path) {
       int fd = -1;
       void *buffer = NULL;
       int result = -1;
       
       /* Open file */
       fd = vfs_open(path, O_RDONLY);
       if (fd < 0) {
           /* errno already set */
           goto cleanup;
       }
       
       /* Allocate buffer */
       buffer = kmalloc(4096);
       if (!buffer) {
           SET_ERRNO_GOTO(THUNDEROS_ENOMEM, cleanup);
       }
       
       /* Perform operation */
       if (do_work(fd, buffer) < 0) {
           /* errno set by do_work */
           goto cleanup;
       }
       
       /* Success */
       result = 0;
       clear_errno();
       
   cleanup:
       if (fd >= 0) vfs_close(fd);
       if (buffer) kfree(buffer);
       return result;
   }

Usage Examples
--------------

VFS Layer
~~~~~~~~~

**Before (magic numbers):**

.. code-block:: c

   int vfs_open(const char *path, uint32_t flags) {
       if (!path) {
           return -1;  /* What error? */
       }
       vfs_node_t *node = vfs_resolve_path(path);
       if (!node) {
           return -1;  /* File not found? Or other error? */
       }
       // ...
   }

**After (errno):**

.. code-block:: c

   int vfs_open(const char *path, uint32_t flags) {
       if (!path) {
           RETURN_ERRNO(THUNDEROS_EINVAL);
       }
       vfs_node_t *node = vfs_resolve_path(path);
       if (!node) {
           RETURN_ERRNO(THUNDEROS_ENOENT);  /* Clear: file not found */
       }
       int fd = vfs_alloc_fd();
       if (fd < 0) {
           RETURN_ERRNO(THUNDEROS_EMFILE);  /* Clear: too many open files */
       }
       // ... success path ...
       clear_errno();
       return fd;
   }

ELF Loader
~~~~~~~~~~

**Before (magic numbers):**

.. code-block:: c

   int elf_load_exec(const char *path, ...) {
       int fd = vfs_open(path, O_RDONLY);
       if (fd < 0) return -1;
       
       if (vfs_read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
           return -2;  /* What does -2 mean? */
       }
       if (ehdr.magic != ELF_MAGIC) {
           return -3;  /* What does -3 mean? */
       }
       // ... returns up to -14 ...
   }

**After (errno):**

.. code-block:: c

   int elf_load_exec(const char *path, ...) {
       int fd = vfs_open(path, O_RDONLY);
       if (fd < 0) {
           /* errno already set by vfs_open */
           return -1;
       }
       
       if (vfs_read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
           vfs_close(fd);
           RETURN_ERRNO(THUNDEROS_EIO);  /* Clear: I/O error */
       }
       
       if (ehdr.magic != ELF_MAGIC) {
           vfs_close(fd);
           RETURN_ERRNO(THUNDEROS_EELF_MAGIC);  /* Clear: invalid ELF magic */
       }
       // ... success ...
       clear_errno();
       return proc->pid;
   }

Debugging with errno
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /* Open file and handle errors */
   int fd = vfs_open("/nonexistent.txt", O_RDONLY);
   if (fd < 0) {
       kernel_perror("vfs_open");
       /* Output: "vfs_open: No such file or directory" */
       
       /* Or check errno directly */
       if (errno == THUNDEROS_ENOENT) {
           hal_uart_puts("File does not exist\n");
       } else if (errno == THUNDEROS_EMFILE) {
           hal_uart_puts("Too many open files\n");
       }
   }

Benefits
--------

**Improved Debuggability**

- Error messages clearly describe what went wrong
- No need to memorize magic number meanings
- ``kernel_perror()`` provides instant error context

**Better Error Propagation**

- Errors naturally propagate up the call stack
- Callers can distinguish between different failure modes
- Error context preserved across function calls

**POSIX Compatibility**

- Familiar API for developers (similar to POSIX errno)
- Easy to port POSIX code to ThunderOS
- Consistent with userspace expectations

**Type Safety**

- Compiler warnings for missing error checks
- Clear separation between return values and error codes
- Enum-like error code organization

Future Enhancements
-------------------

**Userspace errno**

Future versions will expose errno to userspace:

.. code-block:: c

   /* Syscall returns -1 and sets user errno */
   int fd = open("/foo", O_RDONLY);
   if (fd < 0) {
       printf("Error: %s\n", strerror(errno));
   }

**Extended Error Information**

Add detailed error context:

.. code-block:: c

   struct error_info {
       int code;              /* Error code */
       const char *file;      /* Source file */
       int line;              /* Line number */
       const char *function;  /* Function name */
   };

**Error Callbacks**

Register error handlers:

.. code-block:: c

   void set_error_handler(void (*handler)(int errno, const char *msg));

Testing and Validation
-----------------------

Running errno Tests
~~~~~~~~~~~~~~~~~~~

ThunderOS includes comprehensive errno tests that validate error handling across all subsystems.

**Build and Run:**

.. code-block:: bash

   # Build ThunderOS
   make clean && make
   
   # Create test ext2 filesystem
   cd build
   mkdir -p testfs
   echo "Test file" > testfs/test.txt
   mkfs.ext2 -F -q -d testfs ext2-disk.img 10M
   cd ..
   
   # Run with QEMU (IMPORTANT: use virtio-mmio.force-legacy=false)
   qemu-system-riscv64 \
       -machine virt \
       -m 128M \
       -nographic \
       -serial mon:stdio \
       -bios default \
       -kernel build/thunderos.elf \
       -global virtio-mmio.force-legacy=false \
       -drive file=build/ext2-disk.img,if=none,format=raw,id=hd0 \
       -device virtio-blk-device,drive=hd0

**Critical QEMU Configuration:**

The ``-global virtio-mmio.force-legacy=false`` flag is **required** for VirtIO to work correctly in modern (non-legacy) mode. Without this flag:

- VirtIO block device will timeout on all I/O operations
- ext2 filesystem mounting will fail with ``THUNDEROS_EIO``
- All filesystem operations will return ``THUNDEROS_ENOENT`` or ``THUNDEROS_EIO``

This flag ensures QEMU uses the modern VirtIO specification instead of the legacy interface.

**Test Coverage:**

The errno test suite validates:

- Basic errno operations (set/get/clear)
- Error string conversion (``thunderos_strerror``)
- Kernel error printing (``kernel_perror``)
- VFS error propagation
- ELF loader error codes
- ext2 filesystem errors (when filesystem is mounted)
- ``RETURN_ERRNO`` macro functionality
- Error code range validation
- Call stack error propagation
- Multiple consecutive errors

**Expected Output:**

.. code-block:: text

   ========================================
          errno Error Handling Tests
   ========================================
   
   [SETUP] Checking filesystem for errno tests
     [OK] Using pre-mounted ext2 filesystem
   
   [TEST] errno basic operations
     [PASS] errno cleared to 0
     [PASS] errno set to EINVAL
     ...
   
   [TEST] ext2 filesystem errno error codes
     [PASS] ext2_read_inode succeeded
     [PASS] errno cleared on success
     [PASS] ext2_lookup failed for non-existent file
     [PASS] errno set to ENOENT
     ...
   
   ========================================
   Tests passed: 50+, Tests failed: 0
   *** ALL ERRNO TESTS PASSED ***
   ========================================

Common Issues
~~~~~~~~~~~~~

**Issue: "VirtIO block device timeout" or "Failed to mount ext2 filesystem"**

**Cause:** Missing ``-global virtio-mmio.force-legacy=false`` flag in QEMU command.

**Solution:** Always include this flag when running ThunderOS with VirtIO devices.

**Issue: "Filesystem not mounted, skipping ext2 tests"**

**Cause:** VirtIO device not available or ext2 mount failed.

**Solution:** 
1. Verify QEMU is started with VirtIO block device parameters
2. Check that ext2-disk.img exists and is properly formatted
3. Ensure ``-global virtio-mmio.force-legacy=false`` is present

**Issue: errno tests show failures**

**Cause:** Actual bugs in error handling implementation.

**Solution:** Review test output to identify which subsystem is failing and fix the error handling code.

See Also
--------

- :doc:`vfs` - VFS error handling
- :doc:`ext2_filesystem` - ext2 error codes
- :doc:`elf_loader` - ELF loader errors
- :doc:`../api` - Full API reference
