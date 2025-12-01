Virtual Filesystem (VFS)
========================

Overview
--------

The Virtual Filesystem (VFS) layer provides a unified interface for accessing different filesystem types in ThunderOS. It abstracts filesystem-specific implementations (like ext2, FAT32, or future filesystems) behind a common API, allowing applications to work with files without knowing the underlying filesystem format.

This design mirrors the VFS architecture in Unix and Linux systems, providing portability and extensibility.

Architecture
------------

Layered Design
~~~~~~~~~~~~~~

.. code-block:: text

    ┌──────────────────────────────────────────────────┐
    │     User Applications / Shell                    │
    │     (uses vfs_open, vfs_read, vfs_write, etc.)   │
    ├──────────────────────────────────────────────────┤
    │     System Call Layer                            │
    │     (sys_open, sys_read, sys_write)              │
    ├──────────────────────────────────────────────────┤
    │     Virtual Filesystem (VFS) Layer               │
    │     - Path resolution                            │
    │     - Mount point management                     │
    │     - File descriptor allocation                 │
    │     - Dispatch to filesystem operations          │
    ├──────────────────────────────────────────────────┤
    │   ┌─────────────────┬─────────────────┬────────┐ │
    │   │  ext2 Driver    │  FAT32 Driver   │ Future │ │
    │   │  (ext2_vfs.c)   │  (if added)     │  FS    │ │
    │   └─────────────────┴─────────────────┴────────┘ │
    ├──────────────────────────────────────────────────┤
    │     Block Device Layer                           │
    │     (VirtIO block driver, future drivers)        │
    └──────────────────────────────────────────────────┘

Key Concepts
~~~~~~~~~~~~

1. **Mount Points**: Associate filesystem instances with paths (e.g., ``/`` → ext2)
2. **File Descriptors**: Integer handles for open files (0-63)
3. **VFS Operations**: Standardized function pointers for filesystem operations
4. **Path Resolution**: Convert absolute paths to filesystem-specific resources

**VFS Constants:**

.. code-block:: c

    #define VFS_MAX_OPEN_FILES    64    /* Maximum simultaneously open files */
    #define VFS_MAX_PATH          256   /* Maximum path length for VFS */
    #define VFS_FD_STDIN          0     /* Standard input file descriptor */
    #define VFS_FD_STDOUT         1     /* Standard output file descriptor */
    #define VFS_FD_STDERR         2     /* Standard error file descriptor */
    #define VFS_FD_FIRST_REGULAR  3     /* First available FD for files */
    #define VFS_DEFAULT_FILE_MODE 0644  /* Default permissions: rw-r--r-- */

**File Descriptor Allocation:**

File descriptors 0-2 are reserved for standard streams:

- **FD 0 (stdin)**: Standard input (reserved, not yet connected to terminal)
- **FD 1 (stdout)**: Standard output (reserved, not yet connected to terminal)
- **FD 2 (stderr)**: Standard error (reserved, not yet connected to terminal)

Regular files are allocated starting from ``VFS_FD_FIRST_REGULAR`` (3):

.. code-block:: c

    int vfs_alloc_fd(void) {
        for (int i = VFS_FD_FIRST_REGULAR; i < VFS_MAX_OPEN_FILES; i++) {
            if (!g_file_table[i].in_use) {
                g_file_table[i].in_use = 1;
                return i;
            }
        }
        RETURN_ERRNO(THUNDEROS_EMFILE);  // Too many open files
    }

**Default File Permissions:**

When creating files without explicit mode, VFS uses ``VFS_DEFAULT_FILE_MODE`` (0644):

- **Owner**: Read + Write (rw-)
- **Group**: Read only (r--)
- **Others**: Read only (r--)

This follows Unix conventions for secure default permissions.

Data Structures
---------------

VFS Node
~~~~~~~~

Represents a mounted filesystem:

.. code-block:: c

    struct vfs_node {
        char mount_point[256];              // Mount path (e.g., "/", "/mnt")
        struct vfs_operations *ops;         // Filesystem operations
        void *fs_data;                      // Filesystem-specific data
        struct vfs_node *next;              // Linked list of mounts
    };

VFS Operations
~~~~~~~~~~~~~~

Function pointers for filesystem-specific implementations:

.. code-block:: c

    struct vfs_operations {
        // File operations
        int (*open)(void *fs_data, const char *path, int flags);
        int (*close)(void *fs_data, int fd);
        ssize_t (*read)(void *fs_data, int fd, void *buffer, size_t size);
        ssize_t (*write)(void *fs_data, int fd, const void *buffer, size_t size);
        off_t (*seek)(void *fs_data, int fd, off_t offset, int whence);
        
        // Directory operations
        int (*readdir)(void *fs_data, const char *path,
                       void (*callback)(const char *name, uint32_t inode));
        int (*mkdir)(void *fs_data, const char *path, mode_t mode);
        int (*rmdir)(void *fs_data, const char *path);
        
        // File metadata
        int (*stat)(void *fs_data, const char *path, struct stat *st);
        int (*unlink)(void *fs_data, const char *path);
        int (*rename)(void *fs_data, const char *old_path, const char *new_path);
    };

Each filesystem implements these operations. For example, ext2 provides:

.. code-block:: c

    struct vfs_operations ext2_ops = {
        .open = ext2_vfs_open,
        .close = ext2_vfs_close,
        .read = ext2_vfs_read,
        .write = ext2_vfs_write,
        .readdir = ext2_vfs_readdir,
        .stat = ext2_vfs_stat,
        // ... other operations
    };

File Descriptor Table
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #define VFS_MAX_FDS 64
    
    struct vfs_file {
        bool in_use;                // Is this FD allocated?
        struct vfs_node *vfs_node;  // Which filesystem?
        void *private_data;         // FS-specific file data (e.g., inode)
        uint64_t offset;            // Current file position
        int flags;                  // Open flags (O_RDONLY, O_WRONLY, etc.)
        uint32_t inode_num;         // Inode number (if applicable)
    };
    
    static struct vfs_file vfs_file_table[VFS_MAX_FDS];

**File Descriptor Allocation:**

- File descriptors 0-2 are reserved (stdin, stdout, stderr)
- VFS allocates descriptors 3-63 for file operations
- Process-specific file descriptor tables are not yet implemented (global table)

Core Operations
---------------

Mounting a Filesystem
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_mount(const char *mount_point, struct vfs_operations *ops,
                  void *fs_data) {
        // 1. Allocate VFS node
        struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
        if (!node) {
            return -1;
        }
        
        // 2. Initialize node
        strncpy(node->mount_point, mount_point, sizeof(node->mount_point) - 1);
        node->ops = ops;
        node->fs_data = fs_data;
        node->next = NULL;
        
        // 3. Add to mount list (prepend to linked list)
        node->next = vfs_mount_list;
        vfs_mount_list = node;
        
        // 4. Mark root filesystem if mounting at "/"
        if (strcmp(mount_point, "/") == 0) {
            vfs_root = node;
        }
        
        return 0;
    }

**Example: Mount ext2 at root**

.. code-block:: c

    struct ext2_fs *fs = ext2_mount(block_device);
    vfs_mount("/", &ext2_ops, fs);

Path Resolution
~~~~~~~~~~~~~~~

Finds the appropriate VFS node for a given path:

.. code-block:: c

    static struct vfs_node* vfs_resolve_mount(const char *path,
                                               const char **relative_path) {
        struct vfs_node *node = vfs_mount_list;
        struct vfs_node *best_match = NULL;
        size_t best_match_len = 0;
        
        // 1. Find longest matching mount point
        while (node) {
            size_t mp_len = strlen(node->mount_point);
            
            // 2. Check if path starts with mount point
            if (strncmp(path, node->mount_point, mp_len) == 0) {
                // 3. Ensure it's a directory boundary (/ or end of string)
                if (path[mp_len] == '/' || path[mp_len] == '\0' ||
                    strcmp(node->mount_point, "/") == 0) {
                    if (mp_len > best_match_len) {
                        best_match = node;
                        best_match_len = mp_len;
                    }
                }
            }
            
            node = node->next;
        }
        
        // 4. Calculate relative path within filesystem
        if (best_match) {
            if (strcmp(best_match->mount_point, "/") == 0) {
                *relative_path = path;  // Whole path for root
            } else {
                *relative_path = path + best_match_len;
            }
        }
        
        return best_match;
    }

**Example:**

- Path: ``/mnt/usb/data.txt``
- Mount: ``/mnt/usb`` → FAT32 filesystem
- Resolved: FAT32 filesystem, relative path = ``/data.txt``

Opening a File
~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_open(const char *path, int flags) {
        // 1. Resolve path to VFS node
        const char *relative_path;
        struct vfs_node *node = vfs_resolve_mount(path, &relative_path);
        
        if (!node) {
            return -1;  // No filesystem mounted
        }
        
        // 2. Allocate file descriptor
        int fd = -1;
        for (int i = 3; i < VFS_MAX_FDS; i++) {  // Skip stdin/stdout/stderr
            if (!vfs_file_table[i].in_use) {
                fd = i;
                break;
            }
        }
        
        if (fd == -1) {
            return -1;  // No free file descriptors
        }
        
        // 3. Call filesystem-specific open
        int result = node->ops->open(node->fs_data, relative_path, flags);
        if (result < 0) {
            return -1;  // Open failed
        }
        
        // 4. Initialize file descriptor
        vfs_file_table[fd].in_use = true;
        vfs_file_table[fd].vfs_node = node;
        vfs_file_table[fd].offset = 0;
        vfs_file_table[fd].flags = flags;
        vfs_file_table[fd].private_data = NULL;  // FS sets this
        
        return fd;
    }

**Open Flags:**

.. code-block:: c

    #define O_RDONLY    0x0000  // Read-only
    #define O_WRONLY    0x0001  // Write-only
    #define O_RDWR      0x0002  // Read-write
    #define O_CREAT     0x0100  // Create if not exists
    #define O_TRUNC     0x0200  // Truncate to zero length
    #define O_APPEND    0x0400  // Append mode

Reading from a File
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    ssize_t vfs_read(int fd, void *buffer, size_t size) {
        // 1. Validate file descriptor
        if (fd < 0 || fd >= VFS_MAX_FDS || !vfs_file_table[fd].in_use) {
            return -1;
        }
        
        struct vfs_file *file = &vfs_file_table[fd];
        struct vfs_node *node = file->vfs_node;
        
        // 2. Check read permission
        if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR)) {
            return -1;  // Write-only file
        }
        
        // 3. Call filesystem-specific read
        ssize_t bytes_read = node->ops->read(node->fs_data, fd, buffer, size);
        
        // 4. Update file offset (if read succeeded)
        if (bytes_read > 0) {
            file->offset += bytes_read;
        }
        
        return bytes_read;
    }

Writing to a File
~~~~~~~~~~~~~~~~~

.. code-block:: c

    ssize_t vfs_write(int fd, const void *buffer, size_t size) {
        // 1. Validate file descriptor
        if (fd < 0 || fd >= VFS_MAX_FDS || !vfs_file_table[fd].in_use) {
            return -1;
        }
        
        struct vfs_file *file = &vfs_file_table[fd];
        struct vfs_node *node = file->vfs_node;
        
        // 2. Check write permission
        if ((file->flags & O_RDONLY) && !(file->flags & O_RDWR)) {
            return -1;  // Read-only file
        }
        
        // 3. Handle append mode
        if (file->flags & O_APPEND) {
            // Seek to end of file (filesystem handles this)
        }
        
        // 4. Call filesystem-specific write
        ssize_t bytes_written = node->ops->write(node->fs_data, fd, buffer, size);
        
        // 5. Update file offset
        if (bytes_written > 0) {
            file->offset += bytes_written;
        }
        
        return bytes_written;
    }

Seeking
~~~~~~~

.. code-block:: c

    off_t vfs_seek(int fd, off_t offset, int whence) {
        if (fd < 0 || fd >= VFS_MAX_FDS || !vfs_file_table[fd].in_use) {
            return -1;
        }
        
        struct vfs_file *file = &vfs_file_table[fd];
        struct vfs_node *node = file->vfs_node;
        
        // Call filesystem-specific seek (if implemented)
        if (node->ops->seek) {
            off_t new_offset = node->ops->seek(node->fs_data, fd, offset, whence);
            if (new_offset >= 0) {
                file->offset = new_offset;
            }
            return new_offset;
        }
        
        // Default implementation
        switch (whence) {
            case SEEK_SET:
                file->offset = offset;
                break;
            case SEEK_CUR:
                file->offset += offset;
                break;
            case SEEK_END:
                // Would need file size from stat
                return -1;
            default:
                return -1;
        }
        
        return file->offset;
    }

**Seek Modes:**

.. code-block:: c

    #define SEEK_SET  0  // Offset from beginning
    #define SEEK_CUR  1  // Offset from current position
    #define SEEK_END  2  // Offset from end

Closing a File
~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_close(int fd) {
        if (fd < 0 || fd >= VFS_MAX_FDS || !vfs_file_table[fd].in_use) {
            return -1;
        }
        
        struct vfs_file *file = &vfs_file_table[fd];
        struct vfs_node *node = file->vfs_node;
        
        // 1. Call filesystem-specific close (if defined)
        if (node->ops->close) {
            node->ops->close(node->fs_data, fd);
        }
        
        // 2. Free file descriptor
        file->in_use = false;
        file->vfs_node = NULL;
        file->private_data = NULL;
        file->offset = 0;
        
        return 0;
    }

Directory Operations
--------------------

Reading Directory Contents
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_readdir(const char *path,
                    void (*callback)(const char *name, uint32_t inode)) {
        // 1. Resolve path
        const char *relative_path;
        struct vfs_node *node = vfs_resolve_mount(path, &relative_path);
        
        if (!node || !node->ops->readdir) {
            return -1;
        }
        
        // 2. Call filesystem-specific readdir
        return node->ops->readdir(node->fs_data, relative_path, callback);
    }

**Example Usage:**

.. code-block:: c

    void print_entry(const char *name, uint32_t inode) {
        kprintf("%s (inode %d)\n", name, inode);
    }
    
    vfs_readdir("/bin", print_entry);

Creating a Directory
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_mkdir(const char *path, mode_t mode) {
        const char *relative_path;
        struct vfs_node *node = vfs_resolve_mount(path, &relative_path);
        
        if (!node || !node->ops->mkdir) {
            return -1;
        }
        
        return node->ops->mkdir(node->fs_data, relative_path, mode);
    }

File Metadata
~~~~~~~~~~~~~

.. code-block:: c

    int vfs_stat(const char *path, struct stat *st) {
        const char *relative_path;
        struct vfs_node *node = vfs_resolve_mount(path, &relative_path);
        
        if (!node || !node->ops->stat) {
            return -1;
        }
        
        return node->ops->stat(node->fs_data, relative_path, st);
    }

**stat Structure:**

.. code-block:: c

    struct stat {
        uint32_t st_ino;     // Inode number
        mode_t   st_mode;    // File type and permissions
        uint32_t st_nlink;   // Hard link count
        uint32_t st_uid;     // Owner UID
        uint32_t st_gid;     // Group GID
        off_t    st_size;    // File size in bytes
        time_t   st_atime;   // Last access time
        time_t   st_mtime;   // Last modification time
        time_t   st_ctime;   // Last status change time
    };

System Call Integration
-----------------------

VFS operations are exposed to user space via system calls:

.. code-block:: c

    // In kernel/core/syscall.c
    
    ssize_t sys_open(const char *path, int flags) {
        // Validate user pointer
        if (!is_user_address_valid((uint64_t)path)) {
            return -1;
        }
        
        return vfs_open(path, flags);
    }
    
    ssize_t sys_read(int fd, void *buffer, size_t size) {
        if (!is_user_address_valid((uint64_t)buffer)) {
            return -1;
        }
        
        return vfs_read(fd, buffer, size);
    }
    
    ssize_t sys_write(int fd, const void *buffer, size_t size) {
        if (!is_user_address_valid((uint64_t)buffer)) {
            return -1;
        }
        
        return vfs_write(fd, buffer, size);
    }

User programs call these via syscall numbers:

.. code-block:: c

    // In user program
    int fd = syscall(SYS_OPEN, "/test.txt", O_RDONLY);
    char buffer[128];
    ssize_t n = syscall(SYS_READ, fd, buffer, sizeof(buffer));
    syscall(SYS_CLOSE, fd);

Mount Management
----------------

Multiple Mount Points
~~~~~~~~~~~~~~~~~~~~~

VFS supports multiple filesystems mounted at different paths:

.. code-block:: c

    // Mount ext2 at root
    vfs_mount("/", &ext2_ops, ext2_fs);
    
    // Mount FAT32 USB drive at /mnt/usb
    vfs_mount("/mnt/usb", &fat32_ops, fat32_fs);
    
    // Mount tmpfs at /tmp
    vfs_mount("/tmp", &tmpfs_ops, tmpfs_data);

Path resolution always finds the longest matching mount point.

Unmounting
~~~~~~~~~~

.. code-block:: c

    int vfs_unmount(const char *mount_point) {
        struct vfs_node *prev = NULL;
        struct vfs_node *node = vfs_mount_list;
        
        // 1. Find mount point
        while (node) {
            if (strcmp(node->mount_point, mount_point) == 0) {
                // 2. Check if any files are open
                for (int i = 0; i < VFS_MAX_FDS; i++) {
                    if (vfs_file_table[i].in_use &&
                        vfs_file_table[i].vfs_node == node) {
                        return -1;  // Busy
                    }
                }
                
                // 3. Remove from mount list
                if (prev) {
                    prev->next = node->next;
                } else {
                    vfs_mount_list = node->next;
                }
                
                // 4. Clean up
                kfree(node);
                return 0;
            }
            
            prev = node;
            node = node->next;
        }
        
        return -1;  // Not found
    }

Future Enhancements
-------------------

Per-Process File Descriptors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Current:** Global file descriptor table (all processes share FDs)

**Future:** Per-process tables:

.. code-block:: c

    struct process {
        // ... existing fields
        struct vfs_file fd_table[MAX_FDS_PER_PROCESS];
    };

Allows processes to have independent file descriptor spaces.

Pipes and Special Files
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct vfs_operations pipe_ops = {
        .read = pipe_read,
        .write = pipe_write,
        .close = pipe_close,
        // No open, stat, readdir
    };
    
    int pipe(int pipefd[2]) {
        // Create pipe VFS nodes
        // pipefd[0] = read end
        // pipefd[1] = write end
    }

Symbolic Links
~~~~~~~~~~~~~~

.. code-block:: c

    int vfs_readlink(const char *path, char *buffer, size_t size) {
        const char *relative_path;
        struct vfs_node *node = vfs_resolve_mount(path, &relative_path);
        
        if (!node || !node->ops->readlink) {
            return -1;
        }
        
        return node->ops->readlink(node->fs_data, relative_path, buffer, size);
    }

Path resolution would need to follow symlinks recursively.

Memory-Mapped Files
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    void* vfs_mmap(int fd, size_t length, int prot, int flags, off_t offset) {
        // Map file contents into virtual memory
        // Requires MMU integration
    }

Advanced Features
-----------------

File Locking
~~~~~~~~~~~~

.. code-block:: c

    int vfs_flock(int fd, int operation) {
        // LOCK_SH: Shared lock
        // LOCK_EX: Exclusive lock
        // LOCK_UN: Unlock
    }

Access Control
~~~~~~~~~~~~~~

ThunderOS implements POSIX-style file permissions with owner, group, and other access levels.

**Permission Model:**

Each file has a 16-bit mode that includes:

- File type (directory, regular file, etc.)
- Owner permissions (read, write, execute)
- Group permissions (read, write, execute)
- Other permissions (read, write, execute)

**VFS Node Permission Fields:**

.. code-block:: c

    typedef struct vfs_node {
        // ... other fields ...
        uint16_t mode;    /* Permission bits (e.g., 0755) */
        uint16_t uid;     /* Owner user ID */
        uint16_t gid;     /* Owner group ID */
    } vfs_node_t;

**Permission Constants:**

.. code-block:: c

    /* Access check modes */
    #define VFS_ACCESS_READ   4   /* Check read permission */
    #define VFS_ACCESS_WRITE  2   /* Check write permission */
    #define VFS_ACCESS_EXEC   1   /* Check execute permission */

**Permission Checking:**

.. code-block:: c

    int vfs_check_permission(vfs_node_t *node, int access_mode);

The permission check follows standard Unix semantics:

1. If process effective UID is 0 (root), access is always granted
2. If process effective UID matches file owner, use owner permission bits
3. If process effective GID matches file group, use group permission bits
4. Otherwise, use "other" permission bits

**Permission Modification:**

.. code-block:: c

    int vfs_chmod(const char *path, uint32_t new_mode);
    int vfs_chown(const char *path, uint16_t uid, uint16_t gid);

**Example - Permission Check Flow:**

.. code-block:: c

    // File: -rwxr-x--- (0750), uid=1000, gid=1000
    // Process: euid=1000, egid=1000
    
    vfs_check_permission(node, VFS_ACCESS_READ);   // OK (owner has read)
    vfs_check_permission(node, VFS_ACCESS_WRITE);  // OK (owner has write)
    vfs_check_permission(node, VFS_ACCESS_EXEC);   // OK (owner has exec)
    
    // Process: euid=2000, egid=1000
    vfs_check_permission(node, VFS_ACCESS_READ);   // OK (group has read)
    vfs_check_permission(node, VFS_ACCESS_WRITE);  // DENIED (group no write)
    
    // Process: euid=2000, egid=2000
    vfs_check_permission(node, VFS_ACCESS_READ);   // DENIED (other no read)

Debugging
---------

Tracing VFS Operations
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #ifdef VFS_DEBUG
    #define VFS_LOG(fmt, ...) kprintf("[VFS] " fmt "\n", ##__VA_ARGS__)
    #else
    #define VFS_LOG(fmt, ...)
    #endif
    
    int vfs_open(const char *path, int flags) {
        VFS_LOG("open: path=%s, flags=0x%x", path, flags);
        // ... operation
    }

Working Directory Support
~~~~~~~~~~~~~~~~~~~~~~~~~

Each process maintains a current working directory (cwd) for relative path resolution:

.. code-block:: c

    struct process {
        // ... other fields ...
        char cwd[256];  /* Current working directory */
    };

**Key Operations:**

- ``sys_chdir(path)``: Change working directory (syscall 28)
- ``sys_getcwd(buf, size)``: Get current working directory (syscall 29)

**Initialization:**

- Process created: ``cwd = "/"``
- After fork: child inherits parent's cwd

**Current Limitation:**

Only absolute paths are supported. Relative path resolution (``..``, ``subdir``) is not yet implemented.

.. code-block:: c

    // Future: resolve relative paths
    char *vfs_resolve_relative(const char *path, const char *cwd) {
        if (path[0] == '/') {
            return path;  // Already absolute
        }
        // TODO: Handle "..", ".", and relative components
    }

Listing Mount Points
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    void vfs_print_mounts(void) {
        struct vfs_node *node = vfs_mount_list;
        
        kprintf("Mount points:\n");
        while (node) {
            kprintf("  %s -> %p (ops=%p, data=%p)\n",
                    node->mount_point, node, node->ops, node->fs_data);
            node = node->next;
        }
    }

File Descriptor Table
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    void vfs_print_fds(void) {
        kprintf("Open file descriptors:\n");
        for (int i = 0; i < VFS_MAX_FDS; i++) {
            if (vfs_file_table[i].in_use) {
                kprintf("  fd=%d: offset=%lu, flags=0x%x, node=%p\n",
                        i, vfs_file_table[i].offset,
                        vfs_file_table[i].flags,
                        vfs_file_table[i].vfs_node);
            }
        }
    }

References
----------

- `Linux VFS Documentation <https://www.kernel.org/doc/html/latest/filesystems/vfs.html>`_
- `UNIX Filesystem Interface <https://pubs.opengroup.org/onlinepubs/9699919799/>`_
- ThunderOS ext2: ``docs/source/internals/ext2_filesystem.rst``
- System calls: ``docs/source/internals/syscalls.rst``

Implementation Files
--------------------

- ``kernel/fs/vfs.c`` - VFS core implementation
- ``include/fs/vfs.h`` - VFS public API
- ``kernel/fs/ext2_vfs.c`` - ext2 VFS integration
- ``kernel/core/syscall.c`` - System call handlers
