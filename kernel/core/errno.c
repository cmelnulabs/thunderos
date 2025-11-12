/*
 * ThunderOS Error Number (errno) Implementation
 *
 * Provides per-process error tracking and error message strings.
 */

#include "kernel/errno.h"
#include "kernel/process.h"
#include "hal/hal_uart.h"

/* Global errno for early boot (before process management) */
static int g_early_errno = THUNDEROS_OK;

/**
 * Get pointer to current process's errno variable
 */
int *__thunderos_errno_location(void) {
    struct process *current = process_current();
    if (current) {
        return &current->errno_value;
    }
    /* Fallback during early boot */
    return &g_early_errno;
}

/**
 * Set errno and return -1
 */
int set_errno(int error_code) {
    *__thunderos_errno_location() = error_code;
    return -1;
}

/**
 * Get current errno
 */
int get_errno(void) {
    return *__thunderos_errno_location();
}

/**
 * Clear errno
 */
void clear_errno(void) {
    *__thunderos_errno_location() = THUNDEROS_OK;
}

/**
 * Convert error code to string
 */
const char *thunderos_strerror(int error_code) {
    switch (error_code) {
        /* Success */
        case THUNDEROS_OK:           return "Success";
        
        /* Generic errors */
        case THUNDEROS_EPERM:        return "Operation not permitted";
        case THUNDEROS_ENOENT:       return "No such file or directory";
        case THUNDEROS_ESRCH:        return "No such process";
        case THUNDEROS_EINTR:        return "Interrupted system call";
        case THUNDEROS_EIO:          return "I/O error";
        case THUNDEROS_ENXIO:        return "No such device or address";
        case THUNDEROS_E2BIG:        return "Argument list too long";
        case THUNDEROS_ENOEXEC:      return "Exec format error";
        case THUNDEROS_EBADF:        return "Bad file descriptor";
        case THUNDEROS_ECHILD:       return "No child processes";
        case THUNDEROS_EAGAIN:       return "Resource temporarily unavailable";
        case THUNDEROS_ENOMEM:       return "Out of memory";
        case THUNDEROS_EACCES:       return "Permission denied";
        case THUNDEROS_EFAULT:       return "Bad address";
        case THUNDEROS_EBUSY:        return "Device or resource busy";
        case THUNDEROS_EEXIST:       return "File exists";
        case THUNDEROS_EXDEV:        return "Cross-device link";
        case THUNDEROS_ENODEV:       return "No such device";
        case THUNDEROS_ENOTDIR:      return "Not a directory";
        case THUNDEROS_EISDIR:       return "Is a directory";
        case THUNDEROS_EINVAL:       return "Invalid argument";
        case THUNDEROS_ENFILE:       return "File table overflow";
        case THUNDEROS_EMFILE:       return "Too many open files";
        case THUNDEROS_ENOTTY:       return "Inappropriate ioctl for device";
        case THUNDEROS_ETXTBSY:      return "Text file busy";
        case THUNDEROS_EFBIG:        return "File too large";
        case THUNDEROS_ENOSPC:       return "No space left on device";
        case THUNDEROS_ESPIPE:       return "Illegal seek";
        
        /* Filesystem errors */
        case THUNDEROS_EFS_CORRUPT:  return "Filesystem corruption detected";
        case THUNDEROS_EFS_INVAL:    return "Invalid filesystem structure";
        case THUNDEROS_EFS_BADBLK:   return "Bad block number";
        case THUNDEROS_EFS_NOINODE:  return "No free inodes";
        case THUNDEROS_EFS_NOBLK:    return "No free blocks";
        case THUNDEROS_EFS_BADINO:   return "Invalid inode number";
        case THUNDEROS_EFS_BADSUPER: return "Invalid superblock";
        case THUNDEROS_EFS_BADGRP:   return "Invalid block group descriptor";
        case THUNDEROS_EFS_BADDIR:   return "Corrupted directory entry";
        case THUNDEROS_EFS_NOSPACE:  return "Directory full";
        case THUNDEROS_EFS_RDONLY:   return "Read-only filesystem";
        case THUNDEROS_EFS_NOTMNT:   return "Filesystem not mounted";
        
        /* ELF loader errors */
        case THUNDEROS_EELF_MAGIC:   return "Invalid ELF magic number";
        case THUNDEROS_EELF_CLASS:   return "Invalid ELF class (not 64-bit)";
        case THUNDEROS_EELF_DATA:    return "Invalid ELF data encoding";
        case THUNDEROS_EELF_VER:     return "Unsupported ELF version";
        case THUNDEROS_EELF_ARCH:    return "Invalid architecture (not RISC-V)";
        case THUNDEROS_EELF_TYPE:    return "Invalid ELF type (not executable)";
        case THUNDEROS_EELF_PHDR:    return "Invalid program header";
        case THUNDEROS_EELF_LOAD:    return "Failed to load ELF segment";
        case THUNDEROS_EELF_ALIGN:   return "Bad ELF segment alignment";
        case THUNDEROS_EELF_NOPHDR:  return "No program headers found";
        case THUNDEROS_EELF_TOOBIG:  return "ELF file too large";
        
        /* VirtIO/driver errors */
        case THUNDEROS_EVIRTIO_TIMEOUT: return "VirtIO operation timeout";
        case THUNDEROS_EVIRTIO_NACK:    return "VirtIO request failed";
        case THUNDEROS_EVIRTIO_NODEV:   return "VirtIO device not found";
        case THUNDEROS_EVIRTIO_BADDEV:  return "Invalid VirtIO device";
        case THUNDEROS_EVIRTIO_NORING:  return "No VirtIO descriptor rings available";
        case THUNDEROS_EVIRTIO_BADREQ:  return "Invalid VirtIO request";
        case THUNDEROS_EVIRTIO_RESET:   return "VirtIO device reset required";
        case THUNDEROS_EDRV_INIT:       return "Driver initialization failed";
        case THUNDEROS_EDRV_IO:         return "Driver I/O error";
        
        /* Process/scheduler errors */
        case THUNDEROS_EPROC_LIMIT:  return "Process limit reached";
        case THUNDEROS_EPROC_KILLED: return "Process was killed";
        case THUNDEROS_EPROC_ZOMBIE: return "Process is zombie";
        case THUNDEROS_EPROC_NOPROC: return "No such process";
        case THUNDEROS_EPROC_BADPID: return "Invalid process ID";
        case THUNDEROS_EPROC_INIT:   return "Process initialization failed";
        case THUNDEROS_ESCHED_FULL:  return "Scheduler queue full";
        
        /* Memory management errors */
        case THUNDEROS_EMEM_NOMEM:   return "No memory available";
        case THUNDEROS_EMEM_BADPTR:  return "Invalid pointer";
        case THUNDEROS_EMEM_CORRUPT: return "Memory corruption detected";
        case THUNDEROS_EMEM_ALIGN:   return "Invalid alignment";
        case THUNDEROS_EMEM_PERM:    return "Memory permission denied";
        case THUNDEROS_EMEM_FAULT:   return "Page fault";
        case THUNDEROS_EMEM_NOPAGE:  return "No free pages available";
        case THUNDEROS_EMEM_BADPTE:  return "Invalid page table entry";
        case THUNDEROS_EMEM_DMA:     return "DMA allocation failed";
        
        default:
            return "Unknown error";
    }
}

/**
 * Print error message (like POSIX perror)
 */
void kernel_perror(const char *prefix) {
    int err = get_errno();
    
    if (prefix && *prefix) {
        hal_uart_puts(prefix);
        hal_uart_puts(": ");
    }
    
    hal_uart_puts(thunderos_strerror(err));
    hal_uart_puts("\n");
}
