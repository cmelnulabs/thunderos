/*
 * vfs.h - Virtual Filesystem layer
 *
 * Provides abstraction between system calls and filesystem implementations.
 * Supports multiple filesystem types (ext2, future: FAT32, etc.)
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

/* Maximum number of open files per process */
#define VFS_MAX_OPEN_FILES 16

/* Maximum path length */
#define VFS_MAX_PATH 256

/* Default file permissions */
#define VFS_DEFAULT_FILE_MODE 0644  /* rw-r--r-- */

/* File descriptor values */
#define VFS_FD_STDIN  0
#define VFS_FD_STDOUT 1
#define VFS_FD_STDERR 2
#define VFS_FD_FIRST_REGULAR 3  /* First available FD for regular files */

/* File open flags */
#define O_RDONLY  0x0000  /* Read-only */
#define O_WRONLY  0x0001  /* Write-only */
#define O_RDWR    0x0002  /* Read-write */
#define O_CREAT   0x0040  /* Create if not exists */
#define O_TRUNC   0x0200  /* Truncate to zero length */
#define O_APPEND  0x0400  /* Append mode */

/* Seek whence values */
#define SEEK_SET  0  /* Seek from beginning */
#define SEEK_CUR  1  /* Seek from current position */
#define SEEK_END  2  /* Seek from end */

/* File types */
#define VFS_TYPE_FILE      1
#define VFS_TYPE_DIRECTORY 2
#define VFS_TYPE_PIPE      3

/* Forward declarations */
struct vfs_node;
struct vfs_filesystem;

/**
 * Filesystem operations - implemented by each FS type (ext2, etc.)
 */
typedef struct {
    /* Read from file */
    int (*read)(struct vfs_node *node, uint32_t offset, void *buffer, uint32_t size);
    
    /* Write to file */
    int (*write)(struct vfs_node *node, uint32_t offset, const void *buffer, uint32_t size);
    
    /* Open file (optional setup) */
    int (*open)(struct vfs_node *node, uint32_t flags);
    
    /* Close file (optional cleanup) */
    void (*close)(struct vfs_node *node);
    
    /* Lookup file in directory by name */
    struct vfs_node *(*lookup)(struct vfs_node *dir, const char *name);
    
    /* List directory contents */
    int (*readdir)(struct vfs_node *dir, uint32_t index, char *name, uint32_t *inode);
    
    /* Create file */
    int (*create)(struct vfs_node *dir, const char *name, uint32_t mode);
    
    /* Create directory */
    int (*mkdir)(struct vfs_node *dir, const char *name, uint32_t mode);
    
    /* Remove file */
    int (*unlink)(struct vfs_node *dir, const char *name);
    
    /* Remove directory */
    int (*rmdir)(struct vfs_node *dir, const char *name);
} vfs_ops_t;

/**
 * VFS node - represents a file or directory
 */
typedef struct vfs_node {
    char name[256];                    /* File/directory name */
    uint32_t inode;                    /* Inode number */
    uint32_t size;                     /* File size in bytes */
    uint32_t type;                     /* File type (file/dir) */
    uint32_t flags;                    /* Flags */
    struct vfs_filesystem *fs;         /* Filesystem this node belongs to */
    void *fs_data;                     /* Filesystem-specific data */
    vfs_ops_t *ops;                    /* Operations for this node */
} vfs_node_t;

/**
 * Filesystem instance
 */
typedef struct vfs_filesystem {
    char name[32];                     /* Filesystem type name (e.g., "ext2") */
    void *fs_data;                     /* Filesystem-specific data (e.g., ext2_fs_t) */
    vfs_node_t *root;                  /* Root directory node */
    vfs_ops_t *ops;                    /* Default operations */
} vfs_filesystem_t;

/**
 * File descriptor - tracks open file state
 */
typedef struct {
    vfs_node_t *node;                  /* File node (NULL for pipes) */
    uint32_t flags;                    /* Open flags */
    uint32_t pos;                      /* Current file position */
    int in_use;                        /* 1 if FD is allocated */
    void *pipe;                        /* Pipe pointer (if VFS_TYPE_PIPE) */
    uint32_t type;                     /* File type (VFS_TYPE_FILE, VFS_TYPE_PIPE, etc.) */
} vfs_file_t;

/* VFS initialization */
int vfs_init(void);

/* Mount a filesystem at root */
int vfs_mount_root(vfs_filesystem_t *fs);

/* File operations */
int vfs_open(const char *path, uint32_t flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buffer, uint32_t size);
int vfs_write(int fd, const void *buffer, uint32_t size);
int vfs_seek(int fd, int offset, int whence);
int vfs_dup2(int oldfd, int newfd);

/* Directory operations */
int vfs_mkdir(const char *path, uint32_t mode);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);

/* Path resolution */
vfs_node_t *vfs_resolve_path(const char *path);

/* Path normalization - converts relative paths to absolute, resolves . and .. */
int vfs_normalize_path(const char *path, char *normalized, size_t size);

/* File descriptor management */
int vfs_alloc_fd(void);
void vfs_free_fd(int fd);
vfs_file_t *vfs_get_file(int fd);

/* Helper functions */
int vfs_stat(const char *path, uint32_t *size, uint32_t *type);
int vfs_exists(const char *path);

/* Pipe support */
int vfs_create_pipe(int pipefd[2]);

#endif /* VFS_H */
