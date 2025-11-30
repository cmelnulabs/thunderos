/*
 * vfs.c - Virtual Filesystem implementation
 */

#include "../../include/fs/vfs.h"
#include "../../include/hal/hal_uart.h"
#include "../../include/mm/kmalloc.h"
#include "../../include/kernel/errno.h"
#include "../../include/kernel/pipe.h"
#include "../../include/kernel/process.h"
#include <stddef.h>

/* ========================================================================
 * Constants
 * ======================================================================== */

#define PATH_COMPONENT_MAX   64    /* Maximum number of path components */
#define COMPONENT_NAME_MAX   256   /* Maximum length of a single component */

/* ========================================================================
 * Global state
 * ======================================================================== */

/* Global file descriptor table (per-process would be better, but global for now) */
static vfs_file_t g_file_table[VFS_MAX_OPEN_FILES];

/* Root filesystem */
static vfs_filesystem_t *g_root_fs = NULL;

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

static int normalize_build_absolute(const char *path, char *working, size_t working_size);
static int normalize_resolve_components(const char *working, char *normalized, size_t size);

/* ========================================================================
 * Path normalization helpers
 * ======================================================================== */

/**
 * normalize_build_absolute - Build absolute path from relative path and cwd
 * 
 * @param path Input path (relative or absolute)
 * @param working Output buffer for absolute path
 * @param working_size Size of output buffer
 * @return 0 on success, -1 on error (errno set)
 */
static int normalize_build_absolute(const char *path, char *working, size_t working_size) {
    size_t pos = 0;
    
    if (path[0] != '/') {
        /* Relative path - prepend current working directory */
        struct process *current_process = process_current();
        if (current_process && current_process->cwd[0]) {
            const char *cwd = current_process->cwd;
            while (*cwd && pos < working_size - 1) {
                working[pos++] = *cwd++;
            }
        } else {
            /* Default to root if no cwd set */
            working[pos++] = '/';
        }
        
        /* Ensure trailing slash for concatenation */
        if (pos > 0 && working[pos - 1] != '/' && pos < working_size - 1) {
            working[pos++] = '/';
        }
    }
    
    /* Append the input path */
    while (*path && pos < working_size - 1) {
        working[pos++] = *path++;
    }
    working[pos] = '\0';
    
    return 0;
}

/**
 * normalize_resolve_components - Resolve . and .. in path components
 * 
 * Uses a stack-based approach: push regular components, pop for ..
 * 
 * @param working Input absolute path
 * @param normalized Output buffer for resolved path
 * @param size Size of output buffer
 * @return 0 on success, -1 on error (errno set)
 */
static int normalize_resolve_components(const char *working, char *normalized, size_t size) {
    /* Component storage - each component is a null-terminated string */
    static char component_storage[PATH_COMPONENT_MAX][COMPONENT_NAME_MAX];
    int component_count = 0;
    
    const char *cursor = working;
    if (*cursor == '/') {
        cursor++;  /* Skip leading slash */
    }
    
    while (*cursor) {
        /* Find component boundaries */
        const char *component_start = cursor;
        while (*cursor && *cursor != '/') {
            cursor++;
        }
        
        size_t component_length = cursor - component_start;
        
        if (component_length == 0) {
            /* Empty component (double slash), skip */
        } else if (component_length == 1 && component_start[0] == '.') {
            /* Current directory ".", skip */
        } else if (component_length == 2 && component_start[0] == '.' && component_start[1] == '.') {
            /* Parent directory "..", pop if possible */
            if (component_count > 0) {
                component_count--;
            }
            /* At root, silently ignore (can't go above root) */
        } else {
            /* Regular component, copy to storage */
            if (component_count < PATH_COMPONENT_MAX) {
                size_t copy_len = component_length;
                if (copy_len >= COMPONENT_NAME_MAX) {
                    copy_len = COMPONENT_NAME_MAX - 1;
                }
                for (size_t i = 0; i < copy_len; i++) {
                    component_storage[component_count][i] = component_start[i];
                }
                component_storage[component_count][copy_len] = '\0';
                component_count++;
            }
        }
        
        if (*cursor == '/') {
            cursor++;
        }
    }
    
    /* Build the normalized path from components */
    size_t output_pos = 0;
    normalized[output_pos++] = '/';
    
    for (int component_index = 0; component_index < component_count; component_index++) {
        const char *component = component_storage[component_index];
        while (*component && output_pos < size - 1) {
            normalized[output_pos++] = *component++;
        }
        
        /* Add separator between components (not after last) */
        if (component_index < component_count - 1 && output_pos < size - 1) {
            normalized[output_pos++] = '/';
        }
    }
    normalized[output_pos] = '\0';
    
    return 0;
}

/* ========================================================================
 * Public path resolution API
 * ======================================================================== */

/**
 * vfs_normalize_path - Convert relative path to absolute, resolve . and ..
 * 
 * @param path Input path (relative or absolute)
 * @param normalized Output buffer for normalized absolute path
 * @param size Size of output buffer
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid parameters
 */
int vfs_normalize_path(const char *path, char *normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        set_errno(THUNDEROS_EINVAL);
        return -1;
    }
    
    /* Working buffer for intermediate absolute path */
    char working_buffer[VFS_MAX_PATH];
    
    /* Step 1: Build absolute path from relative + cwd */
    if (normalize_build_absolute(path, working_buffer, VFS_MAX_PATH) < 0) {
        return -1;
    }
    
    /* Step 2: Resolve . and .. components */
    if (normalize_resolve_components(working_buffer, normalized, size) < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Initialize VFS
 */
int vfs_init(void) {
    /* Initialize file descriptor table */
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        g_file_table[i].node = NULL;
        g_file_table[i].flags = 0;
        g_file_table[i].pos = 0;
        g_file_table[i].in_use = 0;
        g_file_table[i].pipe = NULL;
        g_file_table[i].type = VFS_TYPE_FILE;
    }
    
    /* Reserve stdin/stdout/stderr */
    g_file_table[VFS_FD_STDIN].in_use = 1;
    g_file_table[VFS_FD_STDOUT].in_use = 1;
    g_file_table[VFS_FD_STDERR].in_use = 1;
    
    g_root_fs = NULL;
    
    hal_uart_puts("vfs: Initialized\n");
    return 0;
}

/**
 * Mount a filesystem at root
 */
int vfs_mount_root(vfs_filesystem_t *fs) {
    if (!fs || !fs->root) {
        hal_uart_puts("vfs: Invalid filesystem\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    g_root_fs = fs;
    hal_uart_puts("vfs: Mounted root filesystem (");
    hal_uart_puts(fs->name);
    hal_uart_puts(")\n");
    clear_errno();
    return 0;
}

/**
 * Allocate a file descriptor
 */
int vfs_alloc_fd(void) {
    for (int i = VFS_FD_FIRST_REGULAR; i < VFS_MAX_OPEN_FILES; i++) {
        if (!g_file_table[i].in_use) {
            g_file_table[i].in_use = 1;
            g_file_table[i].node = NULL;
            g_file_table[i].pos = 0;
            g_file_table[i].flags = 0;
            g_file_table[i].pipe = NULL;
            g_file_table[i].type = VFS_TYPE_FILE;
            return i;
        }
    }
    /* No free descriptors */
    RETURN_ERRNO(THUNDEROS_EMFILE);
}

/**
 * Free a file descriptor
 */
void vfs_free_fd(int fd) {
    if (fd >= 0 && fd < VFS_MAX_OPEN_FILES) {
        g_file_table[fd].in_use = 0;
        g_file_table[fd].node = NULL;
        g_file_table[fd].pos = 0;
        g_file_table[fd].flags = 0;
        g_file_table[fd].pipe = NULL;
        g_file_table[fd].type = VFS_TYPE_FILE;
    }
}

/**
 * Duplicate a file descriptor
 * 
 * Makes newfd be the copy of oldfd, closing newfd first if necessary.
 * 
 * @param oldfd The file descriptor to duplicate
 * @param newfd The target file descriptor number
 * @return newfd on success, -1 on error
 */
int vfs_dup2(int oldfd, int newfd) {
    /* Validate newfd range */
    if (newfd < 0 || newfd >= VFS_MAX_OPEN_FILES) {
        set_errno(THUNDEROS_EINVAL);
        return -1;
    }
    
    /* Get the source file */
    if (oldfd < 0 || oldfd >= VFS_MAX_OPEN_FILES || !g_file_table[oldfd].in_use) {
        set_errno(THUNDEROS_EBADF);
        return -1;
    }
    
    /* If oldfd == newfd, just return newfd */
    if (oldfd == newfd) {
        return newfd;
    }
    
    vfs_file_t *old_file = &g_file_table[oldfd];
    vfs_file_t *new_file = &g_file_table[newfd];
    
    /* Close newfd if it's open */
    if (new_file->in_use) {
        vfs_close(newfd);
    }
    
    /* Copy the file descriptor */
    new_file->node = old_file->node;
    new_file->flags = old_file->flags;
    new_file->pos = old_file->pos;
    new_file->in_use = 1;
    new_file->pipe = old_file->pipe;
    new_file->type = old_file->type;
    
    /* Note: Pipe reference counting is handled by vfs_close */
    
    return newfd;
}

/**
 * Get file structure from descriptor
 */
vfs_file_t *vfs_get_file(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !g_file_table[fd].in_use) {
        set_errno(THUNDEROS_EBADF);
        return NULL;
    }
    return &g_file_table[fd];
}
/**
 * vfs_resolve_path - Resolve a path to a VFS node
 * 
 * Supports both absolute and relative paths. Relative paths are resolved
 * against the current process's working directory.
 * 
 * @param path Path to resolve (absolute or relative)
 * @return VFS node on success, NULL on error (errno set)
 * 
 * @errno THUNDEROS_EFS_NOTMNT - No root filesystem mounted
 * @errno THUNDEROS_EINVAL - Invalid path
 * @errno THUNDEROS_ENOENT - Path component not found
 */
vfs_node_t *vfs_resolve_path(const char *path) {
    if (!g_root_fs) {
        set_errno(THUNDEROS_EFS_NOTMNT);
        return NULL;
    }
    
    if (!path) {
        set_errno(THUNDEROS_EINVAL);
        return NULL;
    }
    
    /* Normalize path (handles relative paths, ., ..) */
    char normalized_path[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized_path, VFS_MAX_PATH) < 0) {
        /* errno already set by vfs_normalize_path */
        return NULL;
    }
    
    /* Root directory special case */
    if (normalized_path[0] == '/' && normalized_path[1] == '\0') {
        return g_root_fs->root;
    }
    
    /* Skip leading slash and walk the path */
    const char *cursor = normalized_path + 1;
    vfs_node_t *current_node = g_root_fs->root;
    char component_name[COMPONENT_NAME_MAX];
    
    while (*cursor) {
        /* Extract path component */
        uint32_t name_index = 0;
        while (*cursor && *cursor != '/') {
            if (name_index < COMPONENT_NAME_MAX - 1) {
                component_name[name_index++] = *cursor;
            }
            cursor++;
        }
        component_name[name_index] = '\0';
        
        if (name_index == 0) {
            /* Skip empty components (shouldn't happen after normalize) */
            if (*cursor == '/') {
                cursor++;
            }
            continue;
        }
        
        /* Lookup component in current directory */
        if (!current_node->ops || !current_node->ops->lookup) {
            set_errno(THUNDEROS_EIO);
            return NULL;
        }
        
        vfs_node_t *next_node = current_node->ops->lookup(current_node, component_name);
        if (!next_node) {
            /* errno already set by lookup */
            return NULL;
        }
        
        current_node = next_node;
        
        /* Skip separator */
        if (*cursor == '/') {
            cursor++;
        }
    }
    
    return current_node;
}

/**
 * Open a file
 */
int vfs_open(const char *path, uint32_t flags) {
    if (!path) {
        hal_uart_puts("vfs: NULL path\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Resolve path */
    vfs_node_t *node = vfs_resolve_path(normalized);
    
    /* If file doesn't exist and O_CREAT is set, create it */
    if (!node && (flags & O_CREAT)) {
        /* Extract parent directory and filename */
        /* For now, only support files in root directory */
        if (normalized[0] == '/' && normalized[1] != '\0') {
            const char *filename = normalized + 1;
            
            /* Check if filename contains '/' */
            const char *p = filename;
            while (*p && *p != '/') p++;
            if (*p == '/') {
                hal_uart_puts("vfs: O_CREAT only supports root directory for now\n");
                RETURN_ERRNO(THUNDEROS_EINVAL);
            }
            
            /* Create file in root directory */
            vfs_node_t *root = g_root_fs->root;
            if (root->ops && root->ops->create) {
                int ret = root->ops->create(root, filename, VFS_DEFAULT_FILE_MODE);
                if (ret != 0) {
                    hal_uart_puts("vfs: Failed to create file\n");
                    /* errno already set by create */
                    return -1;
                }
                
                /* Try to resolve again */
                node = vfs_resolve_path(normalized);
            }
        }
    }
    
    if (!node) {
        hal_uart_puts("vfs: File not found: ");
        hal_uart_puts(path);
        hal_uart_puts("\n");
        RETURN_ERRNO(THUNDEROS_ENOENT);
    }
    
    /* Allocate file descriptor */
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        hal_uart_puts("vfs: No free file descriptors\n");
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Initialize file descriptor */
    g_file_table[fd].node = node;
    g_file_table[fd].flags = flags;
    g_file_table[fd].pos = 0;
    
    /* Call filesystem open if available */
    if (node->ops && node->ops->open) {
        int ret = node->ops->open(node, flags);
        if (ret != 0) {
            vfs_free_fd(fd);
            /* errno already set by open */
            return -1;
        }
    }
    
    /* If O_TRUNC, truncate file to zero */
    if (flags & O_TRUNC) {
        node->size = 0;
    }
    
    /* If O_APPEND, seek to end */
    if (flags & O_APPEND) {
        g_file_table[fd].pos = node->size;
    }
    
    clear_errno();
    return fd;
}

/**
 * Close a file
 */
int vfs_close(int fd) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    /* Handle pipe close */
    if (file->type == VFS_TYPE_PIPE && file->pipe) {
        pipe_t *pipe = (pipe_t*)file->pipe;
        
        /* Close appropriate end based on flags */
        if (file->flags & O_RDONLY) {
            pipe_close_read(pipe);
        } else if (file->flags & O_WRONLY) {
            pipe_close_write(pipe);
        }
        
        /* Free pipe if both ends closed */
        if (pipe_can_free(pipe)) {
            pipe_free(pipe);
        }
    }
    
    /* Call filesystem close if available */
    if (file->node && file->node->ops && file->node->ops->close) {
        file->node->ops->close(file->node);
    }
    
    /* Free the file descriptor */
    vfs_free_fd(fd);
    clear_errno();
    return 0;
}

/**
 * Read from a file
 */
int vfs_read(int fd, void *buffer, uint32_t size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    /* Handle pipe read */
    if (file->type == VFS_TYPE_PIPE) {
        if (!file->pipe) {
            RETURN_ERRNO(THUNDEROS_EINVAL);
        }
        return pipe_read((pipe_t*)file->pipe, buffer, size);
    }
    
    /* Regular file read */
    if (!file->node) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }
    
    /* Check if opened for reading */
    if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for reading\n");
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Check if read operation exists */
    if (!file->node->ops || !file->node->ops->read) {
        hal_uart_puts("vfs: No read operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Read from current position */
    int bytes_read = file->node->ops->read(file->node, file->pos, buffer, size);
    if (bytes_read > 0) {
        file->pos += bytes_read;
    }
    
    return bytes_read;
}

/**
 * Write to a file
 */
int vfs_write(int fd, const void *buffer, uint32_t size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    /* Handle pipe write */
    if (file->type == VFS_TYPE_PIPE) {
        if (!file->pipe) {
            RETURN_ERRNO(THUNDEROS_EINVAL);
        }
        return pipe_write((pipe_t*)file->pipe, buffer, size);
    }
    
    /* Regular file write */
    if (!file->node) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }
    
    /* Check if opened for writing */
    if ((file->flags & O_RDONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for writing\n");
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Check if write operation exists */
    if (!file->node->ops || !file->node->ops->write) {
        hal_uart_puts("vfs: No write operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Write at current position */
    int bytes_written = file->node->ops->write(file->node, file->pos, buffer, size);
    if (bytes_written > 0) {
        file->pos += bytes_written;
        
        /* Update file size if we wrote past end */
        if (file->pos > file->node->size) {
            file->node->size = file->pos;
        }
    }
    
    return bytes_written;
}

/**
 * Seek within a file
 */
int vfs_seek(int fd, int offset, int whence) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file || !file->node) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    uint32_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
            
        case SEEK_CUR:
            new_pos = file->pos + offset;
            break;
            
        case SEEK_END:
            new_pos = file->node->size + offset;
            break;
            
        default:
            hal_uart_puts("vfs: Invalid whence value\n");
            RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    file->pos = new_pos;
    clear_errno();
    return new_pos;
}

/**
 * Create a directory
 */
int vfs_mkdir(const char *path, uint32_t mode) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* For now, only support directories in root */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    const char *dirname = normalized + 1;
    
    /* Check if name contains '/' */
    const char *p = dirname;
    while (*p && *p != '/') p++;
    if (*p == '/') {
        hal_uart_puts("vfs: mkdir only supports root directory for now\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *root = g_root_fs->root;
    if (!root->ops || !root->ops->mkdir) {
        hal_uart_puts("vfs: No mkdir operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    return root->ops->mkdir(root, dirname, mode);
}

/**
 * Remove a directory
 */
int vfs_rmdir(const char *path) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Can't remove root */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Find the last slash to separate parent path and directory name */
    char *last_slash = NULL;
    for (char *p = normalized; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    vfs_node_t *parent_dir;
    const char *dirname;
    
    if (last_slash == normalized) {
        /* Directory is in root (e.g., /emptydir) */
        parent_dir = g_root_fs->root;
        dirname = normalized + 1;
    } else {
        /* Directory is in a subdirectory (e.g., /foo/bar) */
        *last_slash = '\0';  /* Temporarily terminate to get parent path */
        parent_dir = vfs_resolve_path(normalized);
        *last_slash = '/';   /* Restore */
        
        if (!parent_dir) {
            RETURN_ERRNO(THUNDEROS_ENOENT);
        }
        dirname = last_slash + 1;
    }
    
    if (!parent_dir->ops || !parent_dir->ops->rmdir) {
        hal_uart_puts("vfs: No rmdir operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    return parent_dir->ops->rmdir(parent_dir, dirname);
}

/**
 * Remove a file
 */
int vfs_unlink(const char *path) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Must have a filename */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Find the last slash to separate parent path and filename */
    char *last_slash = NULL;
    for (char *p = normalized; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    vfs_node_t *parent_dir;
    const char *filename;
    
    if (last_slash == normalized) {
        /* File is in root (e.g., /deleteme.txt) */
        parent_dir = g_root_fs->root;
        filename = normalized + 1;
    } else {
        /* File is in a subdirectory (e.g., /foo/bar.txt) */
        *last_slash = '\0';  /* Temporarily terminate to get parent path */
        parent_dir = vfs_resolve_path(normalized);
        *last_slash = '/';   /* Restore */
        
        if (!parent_dir) {
            RETURN_ERRNO(THUNDEROS_ENOENT);
        }
        filename = last_slash + 1;
    }
    
    if (!parent_dir->ops || !parent_dir->ops->unlink) {
        hal_uart_puts("vfs: No unlink operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    return parent_dir->ops->unlink(parent_dir, filename);
}

/**
 * Get file status
 */
int vfs_stat(const char *path, uint32_t *size, uint32_t *type) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return -1;
    }
    
    if (size) {
        *size = node->size;
    }
    if (type) {
        *type = node->type;
    }
    
    clear_errno();
    return 0;
}

/**
 * Check if file exists
 */
int vfs_exists(const char *path) {
    vfs_node_t *node = vfs_resolve_path(path);
    return node != NULL;
}

/**
 * Create a pipe and return two file descriptors
 * 
 * Creates an anonymous pipe for inter-process communication.
 * pipefd[0] is the read end, pipefd[1] is the write end.
 * 
 * @param pipefd Array of 2 integers to store file descriptors
 * @return 0 on success, -1 on error
 */
int vfs_create_pipe(int pipefd[2]) {
    if (!pipefd) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Create the pipe */
    pipe_t *pipe = pipe_create();
    if (!pipe) {
        /* errno already set by pipe_create */
        return -1;
    }
    
    /* Allocate read file descriptor */
    int read_fd = vfs_alloc_fd();
    if (read_fd < 0) {
        pipe_free(pipe);
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Allocate write file descriptor */
    int write_fd = vfs_alloc_fd();
    if (write_fd < 0) {
        vfs_free_fd(read_fd);
        pipe_free(pipe);
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Set up read end (pipefd[0]) */
    g_file_table[read_fd].pipe = pipe;
    g_file_table[read_fd].type = VFS_TYPE_PIPE;
    g_file_table[read_fd].flags = O_RDONLY;
    g_file_table[read_fd].node = NULL;
    g_file_table[read_fd].pos = 0;
    
    /* Set up write end (pipefd[1]) */
    g_file_table[write_fd].pipe = pipe;
    g_file_table[write_fd].type = VFS_TYPE_PIPE;
    g_file_table[write_fd].flags = O_WRONLY;
    g_file_table[write_fd].node = NULL;
    g_file_table[write_fd].pos = 0;
    
    /* Return file descriptors */
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    clear_errno();
    return 0;
}
