/*
 * ThunderOS Error Codes
 *
 * POSIX-inspired errno system for consistent error handling across the OS.
 * Each subsystem has a dedicated error code range for easy identification.
 */

#ifndef KERNEL_ERRNO_H
#define KERNEL_ERRNO_H

/*
 * Error Code Ranges:
 * 0       : Success
 * 1-29    : Generic POSIX-style errors
 * 30-49   : Filesystem errors
 * 50-69   : ELF loader errors
 * 70-89   : VirtIO/driver errors
 * 90-109  : Process/scheduler errors
 * 110-129 : Memory management errors
 */

/* ========== Success ========== */
#define THUNDEROS_OK          0   /* No error */

/* ========== Generic Errors (1-29) - POSIX Compatible ========== */
#define THUNDEROS_EPERM       1   /* Operation not permitted */
#define THUNDEROS_ENOENT      2   /* No such file or directory */
#define THUNDEROS_ESRCH       3   /* No such process */
#define THUNDEROS_EINTR       4   /* Interrupted system call */
#define THUNDEROS_EIO         5   /* I/O error */
#define THUNDEROS_ENXIO       6   /* No such device or address */
#define THUNDEROS_E2BIG       7   /* Argument list too long */
#define THUNDEROS_ENOEXEC     8   /* Exec format error */
#define THUNDEROS_EBADF       9   /* Bad file descriptor */
#define THUNDEROS_ECHILD      10  /* No child processes */
#define THUNDEROS_EAGAIN      11  /* Try again / Resource temporarily unavailable */
#define THUNDEROS_ENOMEM      12  /* Out of memory */
#define THUNDEROS_EACCES      13  /* Permission denied */
#define THUNDEROS_EFAULT      14  /* Bad address */
#define THUNDEROS_EBUSY       16  /* Device or resource busy */
#define THUNDEROS_EEXIST      17  /* File exists */
#define THUNDEROS_EXDEV       18  /* Cross-device link */
#define THUNDEROS_ENODEV      19  /* No such device */
#define THUNDEROS_ENOTDIR     20  /* Not a directory */
#define THUNDEROS_EISDIR      21  /* Is a directory */
#define THUNDEROS_EINVAL      22  /* Invalid argument */
#define THUNDEROS_ENFILE      23  /* File table overflow */
#define THUNDEROS_EMFILE      24  /* Too many open files */
#define THUNDEROS_ENOTTY      25  /* Not a typewriter / Inappropriate ioctl */
#define THUNDEROS_ETXTBSY     26  /* Text file busy */
#define THUNDEROS_EFBIG       27  /* File too large */
#define THUNDEROS_ENOSPC      28  /* No space left on device */
#define THUNDEROS_ESPIPE      29  /* Illegal seek */
#define THUNDEROS_EPIPE       32  /* Broken pipe (read end closed) */
#define THUNDEROS_ENOSYS      38  /* Function not implemented */

/* ========== Filesystem Errors (30-49) ========== */
#define THUNDEROS_EFS_CORRUPT 30  /* Filesystem corruption detected */
#define THUNDEROS_EFS_INVAL   31  /* Invalid filesystem structure */
#define THUNDEROS_EFS_BADBLK  32  /* Bad block number */
#define THUNDEROS_EFS_NOINODE 33  /* No free inodes */
#define THUNDEROS_EFS_NOBLK   34  /* No free blocks */
#define THUNDEROS_EFS_BADINO  35  /* Invalid inode number */
#define THUNDEROS_EFS_BADSUPER 36 /* Invalid superblock */
#define THUNDEROS_EFS_BADGRP  37  /* Invalid block group descriptor */
#define THUNDEROS_EFS_BADDIR  38  /* Corrupted directory entry */
#define THUNDEROS_EFS_NOSPACE 39  /* Directory has no space for new entry */
#define THUNDEROS_EFS_RDONLY  40  /* Filesystem is read-only */
#define THUNDEROS_EFS_NOTMNT  41  /* Filesystem not mounted */

/* ========== ELF Loader Errors (50-69) ========== */
#define THUNDEROS_EELF_MAGIC  50  /* Invalid ELF magic number */
#define THUNDEROS_EELF_CLASS  51  /* Wrong ELF class (not 64-bit) */
#define THUNDEROS_EELF_DATA   52  /* Wrong endianness */
#define THUNDEROS_EELF_VER    53  /* Unsupported ELF version */
#define THUNDEROS_EELF_ARCH   54  /* Wrong architecture (not RISC-V) */
#define THUNDEROS_EELF_TYPE   55  /* Wrong ELF type (not executable) */
#define THUNDEROS_EELF_PHDR   56  /* Invalid program header */
#define THUNDEROS_EELF_LOAD   57  /* Failed to load segment */
#define THUNDEROS_EELF_ALIGN  58  /* Bad segment alignment */
#define THUNDEROS_EELF_NOPHDR 59  /* No program headers */
#define THUNDEROS_EELF_TOOBIG 60  /* ELF file too large */

/* ========== VirtIO/Driver Errors (70-89) ========== */
#define THUNDEROS_EVIRTIO_TIMEOUT 70  /* VirtIO operation timeout */
#define THUNDEROS_EVIRTIO_NACK    71  /* VirtIO request failed */
#define THUNDEROS_EVIRTIO_NODEV   72  /* VirtIO device not found */
#define THUNDEROS_EVIRTIO_BADDEV  73  /* Invalid VirtIO device */
#define THUNDEROS_EVIRTIO_NORING  74  /* No available descriptor rings */
#define THUNDEROS_EVIRTIO_BADREQ  75  /* Invalid VirtIO request */
#define THUNDEROS_EVIRTIO_RESET   76  /* Device reset required */
#define THUNDEROS_EDRV_INIT       80  /* Driver initialization failed */
#define THUNDEROS_EDRV_IO         81  /* Driver I/O error */

/* ========== Process/Scheduler Errors (90-109) ========== */
#define THUNDEROS_EPROC_LIMIT  90  /* Process limit reached */
#define THUNDEROS_EPROC_KILLED 91  /* Process was killed */
#define THUNDEROS_EPROC_ZOMBIE 92  /* Process is zombie */
#define THUNDEROS_EPROC_NOPROC 93  /* No such process */
#define THUNDEROS_EPROC_BADPID 94  /* Invalid process ID */
#define THUNDEROS_EPROC_INIT   95  /* Process initialization failed */
#define THUNDEROS_ESCHED_FULL  96  /* Scheduler queue full */

/* ========== Memory Management Errors (110-129) ========== */
#define THUNDEROS_EMEM_NOMEM   110 /* No memory available */
#define THUNDEROS_EMEM_BADPTR  111 /* Invalid pointer */
#define THUNDEROS_EMEM_CORRUPT 112 /* Memory corruption detected */
#define THUNDEROS_EMEM_ALIGN   113 /* Bad alignment */
#define THUNDEROS_EMEM_PERM    114 /* Permission denied (page protection) */
#define THUNDEROS_EMEM_FAULT   115 /* Page fault */
#define THUNDEROS_EMEM_NOPAGE  116 /* No free pages */
#define THUNDEROS_EMEM_BADPTE  117 /* Invalid page table entry */
#define THUNDEROS_EMEM_DMA     118 /* DMA allocation failed */

/* ========== Error Handling Functions ========== */

/**
 * Get pointer to current process's errno variable
 * 
 * Returns per-process errno for current process, or global errno
 * during early boot before process management is initialized.
 * 
 * @return Pointer to errno variable
 */
int *__thunderos_errno_location(void);

/**
 * Set errno and return error indicator (-1)
 * 
 * Convenience function for:
 *   errno = error_code;
 *   return -1;
 * 
 * @param error_code Error code to set
 * @return Always returns -1
 * 
 * Example:
 *   if (!buffer) return set_errno(THUNDEROS_ENOMEM);
 */
int set_errno(int error_code);

/**
 * Get current errno value
 * 
 * @return Current error code
 */
int get_errno(void);

/**
 * Clear errno (set to THUNDEROS_OK)
 * 
 * Should be called at start of successful operations.
 */
void clear_errno(void);

/**
 * Convert error code to human-readable string
 * 
 * @param error_code Error code
 * @return Error message string (never NULL)
 * 
 * Example:
 *   hal_uart_puts(thunderos_strerror(errno));
 */
const char *thunderos_strerror(int error_code);

/**
 * Print error message to console (like POSIX perror)
 * 
 * Prints: "<prefix>: <error message>\n"
 * 
 * @param prefix Prefix string (can be NULL)
 * 
 * Example:
 *   if (vfs_open("/foo", O_RDONLY) < 0) {
 *       kernel_perror("vfs_open");  // Prints: "vfs_open: No such file or directory"
 *   }
 */
void kernel_perror(const char *prefix);

/* ========== Error Handling Macros ========== */

/**
 * Set errno and return -1
 * 
 * Usage:
 *   if (!valid) RETURN_ERRNO(THUNDEROS_EINVAL);
 */
#define RETURN_ERRNO(err) return set_errno(err)

/**
 * Set errno and return NULL
 * 
 * Usage:
 *   if (!buffer) RETURN_ERRNO_NULL(THUNDEROS_ENOMEM);
 */
#define RETURN_ERRNO_NULL(err) do { \
    set_errno(err); \
    return NULL; \
} while(0)

/**
 * Set errno and goto cleanup label
 * 
 * Usage:
 *   if (alloc_failed) SET_ERRNO_GOTO(THUNDEROS_ENOMEM, cleanup);
 */
#define SET_ERRNO_GOTO(err, label) do { \
    set_errno(err); \
    goto label; \
} while(0)

/**
 * Convenient errno access (like POSIX)
 * 
 * Usage:
 *   errno = THUNDEROS_EINVAL;
 *   if (errno == THUNDEROS_ENOMEM) { ... }
 */
#define errno (*__thunderos_errno_location())

#endif // KERNEL_ERRNO_H
