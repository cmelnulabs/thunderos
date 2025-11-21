/*
 * System Call Handler for ThunderOS
 * 
 * Implements the system call interface for user-mode processes.
 */

#include "kernel/syscall.h"
#include "hal/hal_timer.h"
#include "mm/paging.h"
#include "kernel/process.h"
#include "hal/hal_uart.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include "kernel/elf_loader.h"
#include "fs/vfs.h"
#include <stdint.h>
#include <stddef.h>

// Memory protection flags for mmap
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

// Mapping flags for mmap
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20

// Constants
#define KERNEL_SPACE_START 0x8000000000000000UL
#define STDIN_FD  0
#define STDOUT_FD 1
#define STDERR_FD 2
#define SYSCALL_ERROR ((uint64_t)-1)
#define SYSCALL_SUCCESS 0

// Forward declarations
static int is_valid_user_pointer(const void *pointer, size_t length);

/**
 * is_valid_user_pointer - Validate user-space pointer with memory isolation
 * 
 * Performs comprehensive validation using process VMAs to ensure pointer
 * is within a mapped memory region with appropriate permissions.
 * 
 * @param pointer User-space pointer to validate
 * @param length Length of memory region in bytes
 * @return 1 if valid, 0 if invalid
 */
static int is_valid_user_pointer(const void *pointer, size_t length) {
    struct process *proc = process_current();
    
    if (!proc || !pointer || length == 0) {
        return 0;
    }
    
    // Basic validation
    uintptr_t address = (uintptr_t)pointer;
    
    // User addresses must be in lower half (below kernel space)
    if (address >= KERNEL_SPACE_START) {
        return 0;
    }
    
    // Check for address overflow
    uintptr_t end_address = address + length;
    if (end_address < address) {
        return 0;
    }
    
    // Validate against process VMAs (memory isolation check)
    return process_validate_user_ptr(proc, pointer, length, VM_USER);
}

/**
 * sys_exit - Terminate the current process
 * 
 * @param status Exit status code
 * @return Never returns
 */
uint64_t sys_exit(int status) {
    process_exit(status);
    // Never reaches here
    return 0;
}

/**
 * sys_waitpid - Wait for a child process to change state
 * 
 * @param pid Process ID to wait for:
 *            -1 = wait for any child
 *            >0 = wait for specific child PID
 * @param wstatus Pointer to store exit status (can be NULL)
 * @param options Options (0 for blocking wait)
 * @return PID of terminated child, or -1 on error
 */
uint64_t sys_waitpid(int pid, int *wstatus, int options) {
    (void)options;  // Options not implemented yet
    
    struct process *current = process_current();
    if (!current) {
        return SYSCALL_ERROR;
    }
    
    // Find zombie child process
    while (1) {
        struct process *child = NULL;
        int found_child = 0;
        
        // Search for matching child process
        extern struct process *process_find_zombie_child(struct process *parent, int target_pid);
        child = process_find_zombie_child(current, pid);
        
        if (child) {
            // Found zombie child - reap it
            int exit_code = child->exit_code;
            pid_t child_pid = child->pid;
            
            // Store exit status if requested
            if (wstatus) {
                *wstatus = (exit_code & 0xFF) << 8;  // Linux-style status encoding
            }
            
            // Free child process resources
            process_free(child);
            
            return child_pid;
        }
        
        // Check if parent has any children at all
        extern int process_has_children(struct process *parent, int target_pid);
        found_child = process_has_children(current, pid);
        
        if (!found_child) {
            // No such child process
            return SYSCALL_ERROR;
        }
        
        // Child exists but hasn't exited yet - yield and try again
        scheduler_yield();
    }
}

/**
 * sys_getpid - Get current process ID
 * 
 * @return Current process ID, or -1 on error
 */
uint64_t sys_getpid(void) {
    struct process *current_process = process_current();
    
    if (current_process == NULL) {
        return SYSCALL_ERROR;
    }
    
    return current_process->pid;
}

/**
 * sys_sbrk - Adjust heap size with memory isolation
 * 
 * Implements heap expansion/contraction with complete memory isolation.
 * Allocates physical pages, maps them into process address space, and
 * updates VMA tracking.
 * 
 * @param heap_increment Bytes to add to heap (can be negative)
 * @return Previous heap end on success, or -1 on error
 */
uint64_t sys_sbrk(int heap_increment) {
    struct process *proc = process_current();
    if (!proc) {
        return SYSCALL_ERROR;
    }
    
    uint64_t old_brk = proc->heap_end;
    
    // If increment is zero, just return current brk
    if (heap_increment == 0) {
        return old_brk;
    }
    
    uint64_t new_brk = old_brk + heap_increment;
    
    // Validate new break is within reasonable bounds
    if (new_brk < proc->heap_start) {
        return SYSCALL_ERROR;  // Can't shrink below heap start
    }
    
    // Don't let heap grow into stack (leave safety margin)
    uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    if (new_brk >= stack_bottom - HEAP_STACK_SAFETY_MARGIN) {
        return SYSCALL_ERROR;
    }
    
    // Expanding heap
    if (heap_increment > 0) {
        // Calculate pages needed
        uint64_t old_page = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t new_page = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        
        // Map new pages if crossing page boundary
        if (new_page > old_page) {
            if (process_map_region(proc, old_page, new_page - old_page, 
                                 VM_READ | VM_WRITE | VM_USER) != 0) {
                return SYSCALL_ERROR;
            }
        }
    }
    // Shrinking heap
    else {
        // Calculate pages to unmap
        uint64_t new_page = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t old_page = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        
        // Unmap pages if crossing page boundary
        for (uint64_t addr = new_page; addr < old_page; addr += PAGE_SIZE) {
            unmap_page(proc->page_table, addr);
        }
        
        // Update VMA for heap
        vm_area_t *heap_vma = process_find_vma(proc, proc->heap_start);
        if (heap_vma && heap_vma->start == proc->heap_start) {
            heap_vma->end = new_page;
        }
    }
    
    // Update heap end
    proc->heap_end = new_brk;
    
    return old_brk;
}

/**
 * sys_sleep - Sleep for specified milliseconds
 * 
 * @param milliseconds Milliseconds to sleep
 * @return 0 on success
 */
uint64_t sys_sleep(uint64_t milliseconds) {
    (void)milliseconds;  // Suppress unused parameter warning
    
    // Not fully implemented - just yield for now
    // TODO: Implement proper sleep with timer-based wakeup
    process_yield();
    return SYSCALL_SUCCESS;
}

/**
 * sys_yield - Yield CPU to another process
 * 
 * @return 0 on success
 */
uint64_t sys_yield(void) {
    process_yield();
    return SYSCALL_SUCCESS;
}

/**
 * sys_getppid - Get parent process ID
 * 
 * @return Parent process ID, or 0 if no parent
 */
uint64_t sys_getppid(void) {
    struct process *current_process = process_current();
    
    if (current_process == NULL) {
        return 0;
    }
    
    // Return ppid if available (currently PCB doesn't have ppid field)
    // For now, return 0 (init process has no parent)
    // TODO: Add ppid field to PCB structure
    return 0;
}

/**
 * sys_kill - Send signal to process
 * 
 * @param pid Target process ID
 * @param signal Signal number (ignored for now)
 * @return 0 on success, -1 on error
 */
uint64_t sys_kill(int pid, int signal) {
    (void)signal;  // Signals not implemented yet
    
    if (pid <= 0) {
        return SYSCALL_ERROR;
    }
    
    // Find process by PID and terminate it
    // TODO: Implement process_find_by_pid() and proper signal handling
    // For now, just return error
    return SYSCALL_ERROR;
}

/**
 * sys_gettime - Get system time
 * 
 * @return Milliseconds since boot
 */
uint64_t sys_gettime(void) {
    // Get time from hardware timer
    extern uint64_t hal_timer_get_ticks(void);
    uint64_t ticks = hal_timer_get_ticks();
    
    // Convert ticks to milliseconds
    return ticks / TICKS_PER_MS;
}

/**
 * sys_open - Open a file
 * 
 * @param path File path
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.)
 * @param mode File permissions (for O_CREAT)
 * @return File descriptor on success, -1 on error
 */
uint64_t sys_open(const char *path, int flags, int mode) {
    if (!is_valid_user_pointer(path, 1)) {
        return SYSCALL_ERROR;
    }
    
    // Validate path length
    size_t path_len = 0;
    const char *p = path;
    while (*p && path_len < 4096) {
        p++;
        path_len++;
    }
    
    if (path_len == 0 || path_len >= 4096) {
        return SYSCALL_ERROR;
    }
    
    // Convert flags to VFS flags
    int vfs_flags = 0;
    if ((flags & O_RDWR) == O_RDWR) {
        vfs_flags = O_RDWR;
    } else if (flags & O_WRONLY) {
        vfs_flags = O_WRONLY;
    } else {
        vfs_flags = O_RDONLY;
    }
    
    if (flags & O_CREAT) {
        vfs_flags |= O_CREAT;
    }
    if (flags & O_TRUNC) {
        vfs_flags |= O_TRUNC;
    }
    if (flags & O_APPEND) {
        vfs_flags |= O_APPEND;
    }
    
    int fd = vfs_open(path, vfs_flags);
    if (fd < 0) {
        return SYSCALL_ERROR;
    }
    
    (void)mode; // TODO: Use mode for file permissions
    return fd;
}

/**
 * sys_close - Close a file descriptor
 * 
 * @param fd File descriptor to close
 * @return 0 on success, -1 on error
 */
uint64_t sys_close(int fd) {
    // Don't allow closing stdin/stdout/stderr
    if (fd <= STDERR_FD) {
        return SYSCALL_ERROR;
    }
    
    int result = vfs_close(fd);
    return (result == 0) ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

/**
 * sys_read - Read data from a file descriptor
 * 
 * Enhanced version with memory isolation validation.
 * Validates buffer is in mapped memory with write permissions.
 * 
 * @param file_descriptor File descriptor
 * @param buffer Buffer to read into
 * @param byte_count Maximum number of bytes to read
 * @return Number of bytes read, or -1 on error
 */
uint64_t sys_read(int file_descriptor, char *buffer, size_t byte_count) {
    struct process *proc = process_current();
    
    // Validate user buffer with write permission (we're writing to it)
    if (!process_validate_user_ptr(proc, buffer, byte_count, VM_WRITE | VM_USER)) {
        return SYSCALL_ERROR;
    }
    
    // Handle stdin separately
    if (file_descriptor == STDIN_FD) {
        // Not implemented yet - requires input buffering
        return 0;
    }
    
    // Handle regular file descriptors
    if (file_descriptor <= STDERR_FD) {
        return SYSCALL_ERROR;
    }
    
    int bytes_read = vfs_read(file_descriptor, buffer, byte_count);
    if (bytes_read < 0) {
        return SYSCALL_ERROR;
    }
    
    return bytes_read;
}

/**
 * sys_write - Write data to a file descriptor
 * 
 * Enhanced version with memory isolation validation.
 * Validates buffer is in mapped memory with read permissions.
 * 
 * @param file_descriptor File descriptor
 * @param buffer Buffer to write from
 * @param byte_count Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
uint64_t sys_write(int file_descriptor, const char *buffer, size_t byte_count) {
    struct process *proc = process_current();
    
    // Validate user buffer with read permission (we're reading from it)
    if (!process_validate_user_ptr(proc, buffer, byte_count, VM_READ | VM_USER)) {
        return SYSCALL_ERROR;
    }
    
    // Handle stdout/stderr with UART
    if (file_descriptor == STDOUT_FD || file_descriptor == STDERR_FD) {
        int bytes_written = hal_uart_write(buffer, byte_count);
        if (bytes_written != (int)byte_count) {
            return SYSCALL_ERROR;
        }
        return byte_count;
    }
    
    // Handle stdin (cannot write)
    if (file_descriptor == STDIN_FD) {
        return SYSCALL_ERROR;
    }
    
    // Handle regular file descriptors
    int bytes_written = vfs_write(file_descriptor, buffer, byte_count);
    if (bytes_written < 0) {
        return SYSCALL_ERROR;
    }
    
    return bytes_written;
}

/**
 * sys_lseek - Seek file position
 * 
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New file position, or -1 on error
 */
uint64_t sys_lseek(int fd, int64_t offset, int whence) {
    // Don't allow seeking on stdin/stdout/stderr
    if (fd <= STDERR_FD) {
        return SYSCALL_ERROR;
    }
    
    // Convert whence to VFS whence
    int vfs_whence;
    switch (whence) {
        case SEEK_SET:
            vfs_whence = SEEK_SET;
            break;
        case SEEK_CUR:
            vfs_whence = SEEK_CUR;
            break;
        case SEEK_END:
            vfs_whence = SEEK_END;
            break;
        default:
            return SYSCALL_ERROR;
    }
    
    int64_t new_pos = vfs_seek(fd, offset, vfs_whence);
    if (new_pos < 0) {
        return SYSCALL_ERROR;
    }
    
    return new_pos;
}

/**
 * sys_stat - Get file status
 * 
 * @param path File path
 * @param statbuf Buffer to store stat information (size and type)
 * @return 0 on success, -1 on error
 */
uint64_t sys_stat(const char *path, void *statbuf) {
    if (!is_valid_user_pointer(path, 1) || !is_valid_user_pointer(statbuf, 8)) {
        return SYSCALL_ERROR;
    }
    
    // Validate path length
    size_t path_len = 0;
    const char *p = path;
    while (*p && path_len < SYSCALL_MAX_PATH) {
        p++;
        path_len++;
    }
    
    if (path_len == 0 || path_len >= SYSCALL_MAX_PATH) {
        return SYSCALL_ERROR;
    }
    
    uint32_t *stat_data = (uint32_t *)statbuf;
    int result = vfs_stat(path, &stat_data[0], &stat_data[1]);
    return (result == 0) ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

/**
 * sys_mkdir - Create a directory
 * 
 * @param path Directory path
 * @param mode Directory permissions
 * @return 0 on success, -1 on error
 */
uint64_t sys_mkdir(const char *path, int mode) {
    if (!is_valid_user_pointer(path, 1)) {
        return SYSCALL_ERROR;
    }
    
    // Validate path length
    size_t path_len = 0;
    const char *p = path;
    while (*p && path_len < SYSCALL_MAX_PATH) {
        p++;
        path_len++;
    }
    
    if (path_len == 0 || path_len >= SYSCALL_MAX_PATH) {
        return SYSCALL_ERROR;
    }
    
    int result = vfs_mkdir(path, mode);
    return (result == 0) ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

/**
 * sys_unlink - Remove a file
 * 
 * @param path File path
 * @return 0 on success, -1 on error
 */
uint64_t sys_unlink(const char *path) {
    if (!is_valid_user_pointer(path, 1)) {
        return SYSCALL_ERROR;
    }
    
    // Validate path length
    size_t path_len = 0;
    const char *p = path;
    while (*p && path_len < SYSCALL_MAX_PATH) {
        p++;
        path_len++;
    }
    
    if (path_len == 0 || path_len >= SYSCALL_MAX_PATH) {
        return SYSCALL_ERROR;
    }
    
    int result = vfs_unlink(path);
    return (result == 0) ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

/**
 * sys_rmdir - Remove a directory
 * 
 * @param path Directory path
 * @return 0 on success, -1 on error
 */
uint64_t sys_rmdir(const char *path) {
    if (!is_valid_user_pointer(path, 1)) {
        return SYSCALL_ERROR;
    }
    
    // Validate path length
    size_t path_len = 0;
    const char *p = path;
    while (*p && path_len < SYSCALL_MAX_PATH) {
        p++;
        path_len++;
    }
    
    if (path_len == 0 || path_len >= SYSCALL_MAX_PATH) {
        return SYSCALL_ERROR;
    }
    
    int result = vfs_rmdir(path);
    return (result == 0) ? SYSCALL_SUCCESS : SYSCALL_ERROR;
}

/**
 * sys_mmap - Map memory into process address space
 * 
 * Simplified mmap implementation for memory isolation.
 * 
 * @param addr Hint address (0 = kernel chooses)
 * @param length Length of mapping in bytes
 * @param prot Protection flags (PROT_READ, PROT_WRITE, PROT_EXEC)
 * @param flags Mapping flags (MAP_PRIVATE, MAP_ANONYMOUS, etc.)
 * @param fd File descriptor (ignored if MAP_ANONYMOUS)
 * @param offset File offset (ignored if MAP_ANONYMOUS)
 * @return Mapped address on success, -1 on error
 */
uint64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset) {
    struct process *proc = process_current();
    if (!proc || length == 0) {
        return SYSCALL_ERROR;
    }
    
    (void)fd;      // TODO: Implement file-backed mappings
    (void)offset;  // TODO: Implement file offset
    (void)flags;   // TODO: Handle MAP_SHARED vs MAP_PRIVATE
    
    // Determine mapping address
    uint64_t map_addr;
    if (addr) {
        map_addr = (uint64_t)addr;
    } else {
        // Find free space in user address space (simple allocator)
        map_addr = USER_MMAP_START;
        
        // Search for free region
        vm_area_t *vma = proc->vm_areas;
        while (vma) {
            if (map_addr >= vma->start && map_addr < vma->end) {
                map_addr = vma->end;
            }
            vma = vma->next;
        }
    }
    
    // Convert protection flags
    uint32_t vm_flags = VM_USER;
    if (prot & PROT_READ) vm_flags |= VM_READ;
    if (prot & PROT_WRITE) vm_flags |= VM_WRITE;
    if (prot & PROT_EXEC) vm_flags |= VM_EXEC;
    
    // Map the region
    if (process_map_region(proc, map_addr, length, vm_flags) != 0) {
        return SYSCALL_ERROR;
    }
    
    return map_addr;
}

/**
 * sys_munmap - Unmap memory from process address space
 * 
 * @param addr Address to unmap (must be page-aligned)
 * @param length Length of mapping in bytes
 * @return 0 on success, -1 on error
 */
uint64_t sys_munmap(void *addr, size_t length) {
    struct process *proc = process_current();
    if (!proc || !addr || length == 0) {
        return SYSCALL_ERROR;
    }
    
    uint64_t start = (uint64_t)addr & ~(PAGE_SIZE - 1);
    uint64_t end = ((uint64_t)addr + length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Find and remove VMA
    vm_area_t *vma = process_find_vma(proc, start);
    if (!vma || vma->start != start) {
        return SYSCALL_ERROR;  // Address not start of mapping
    }
    
    // Unmap all pages in the region
    for (uint64_t page = start; page < end; page += PAGE_SIZE) {
        unmap_page(proc->page_table, page);
    }
    
    // Remove VMA
    process_remove_vma(proc, vma);
    
    return SYSCALL_SUCCESS;
}

/**
 * sys_pipe - Create a pipe
 * 
 * Creates an anonymous pipe for inter-process communication.
 * Returns two file descriptors in pipefd array:
 * - pipefd[0] is the read end
 * - pipefd[1] is the write end
 * 
 * Typical usage with fork():
 *   int pipefd[2];
 *   pipe(pipefd);
 *   if (fork() == 0) {
 *     // Child: close read end, write to pipe
 *     close(pipefd[0]);
 *     write(pipefd[1], "hello", 5);
 *   } else {
 *     // Parent: close write end, read from pipe
 *     close(pipefd[1]);
 *     read(pipefd[0], buf, sizeof(buf));
 *   }
 * 
 * @param pipefd Array of 2 integers to receive file descriptors
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid pipefd pointer
 * @errno THUNDEROS_EMFILE - Too many open files
 * @errno THUNDEROS_ENOMEM - Failed to allocate pipe buffer
 */
uint64_t sys_pipe(int pipefd[2]) {
    struct process *proc = process_current();
    if (!proc) {
        return SYSCALL_ERROR;
    }
    
    // Validate user pointer
    if (!pipefd || !process_validate_user_ptr(proc, pipefd, sizeof(int) * 2, VM_WRITE)) {
        return SYSCALL_ERROR;
    }
    
    // Create the pipe through VFS
    if (vfs_create_pipe(pipefd) != 0) {
        // errno already set by vfs_create_pipe
        return SYSCALL_ERROR;
    }
    
    return SYSCALL_SUCCESS;
}

/**
 * sys_execve - Execute program from filesystem
 * 
 * @param path Path to executable
 * @param argv Argument array
 * @param envp Environment array (ignored)
 * @return Does not return on success, -1 on error
 */
uint64_t sys_execve(const char *path, const char *argv[], const char *envp[]) {
    (void)envp;
    
    if (!is_valid_user_pointer(path, 1)) {
        return SYSCALL_ERROR;
    }
    
    // Count arguments
    int argc = 0;
    if (argv) {
        while (argv[argc] && argc < SYSCALL_MAX_ARGC) {
            if (!is_valid_user_pointer(argv[argc], 1)) {
                return SYSCALL_ERROR;
            }
            argc++;
        }
    }
    
    // Load and execute ELF binary
    int result = elf_load_exec(path, argv, argc);
    
    // If we get here, exec failed
    return (result < 0) ? SYSCALL_ERROR : (uint64_t)result;
}

/**
 * syscall_handler - Main system call dispatcher
 * 
 * Called from trap handler when ECALL is executed from user mode.
 * Dispatches to appropriate syscall implementation based on syscall number.
 * 
 * @param syscall_number Syscall number (from a7 register)
 * @param argument0 First argument (from a0 register)
 * @param argument1 Second argument (from a1 register)
 * @param argument2 Third argument (from a2 register)
 * @param argument3 Fourth argument (from a3 register)
 * @param argument4 Fifth argument (from a4 register)
 * @param argument5 Sixth argument (from a5 register)
 * @return Return value (placed in a0 register)
 */
uint64_t syscall_handler(uint64_t syscall_number, 
                        uint64_t argument0, uint64_t argument1, uint64_t argument2,
                        uint64_t argument3, uint64_t argument4, uint64_t argument5) {
    uint64_t return_value = SYSCALL_ERROR;
    
    (void)argument3;  // Suppress unused parameter warnings
    (void)argument4;
    (void)argument5;
    
    switch (syscall_number) {
        case SYS_EXIT:
            return_value = sys_exit((int)argument0);
            break;
        
        case SYS_WAIT:  // waitpid
            return_value = sys_waitpid((int)argument0, (int *)argument1, (int)argument2);
            break;
            
        case SYS_WRITE:
            return_value = sys_write((int)argument0, (const char *)argument1, (size_t)argument2);
            break;
            
        case SYS_READ:
            return_value = sys_read((int)argument0, (char *)argument1, (size_t)argument2);
            break;
            
        case SYS_GETPID:
            return_value = sys_getpid();
            break;
            
        case SYS_SBRK:
            return_value = sys_sbrk((int)argument0);
            break;
            
        case SYS_SLEEP:
            return_value = sys_sleep(argument0);
            break;
            
        case SYS_YIELD:
            return_value = sys_yield();
            break;
            
        case SYS_GETPPID:
            return_value = sys_getppid();
            break;
            
        case SYS_KILL:
            return_value = sys_kill((int)argument0, (int)argument1);
            break;
            
        case SYS_GETTIME:
            return_value = sys_gettime();
            break;
            
        case SYS_OPEN:
            return_value = sys_open((const char *)argument0, (int)argument1, (int)argument2);
            break;
            
        case SYS_CLOSE:
            return_value = sys_close((int)argument0);
            break;
            
        case SYS_LSEEK:
            return_value = sys_lseek((int)argument0, (int64_t)argument1, (int)argument2);
            break;
            
        case SYS_STAT:
            return_value = sys_stat((const char *)argument0, (void *)argument1);
            break;
            
        case SYS_MKDIR:
            return_value = sys_mkdir((const char *)argument0, (int)argument1);
            break;
            
        case SYS_UNLINK:
            return_value = sys_unlink((const char *)argument0);
            break;
            
        case SYS_RMDIR:
            return_value = sys_rmdir((const char *)argument0);
            break;
            
        case SYS_EXECVE:
            return_value = sys_execve((const char *)argument0, (const char **)argument1, (const char **)argument2);
            break;
            
        case SYS_MMAP:
            return_value = sys_mmap((void *)argument0, (size_t)argument1, (int)argument2, 
                                   (int)argument3, (int)argument4, (uint64_t)argument5);
            break;
            
        case SYS_MUNMAP:
            return_value = sys_munmap((void *)argument0, (size_t)argument1);
            break;
            
        case SYS_PIPE:
            return_value = sys_pipe((int *)argument0);
            break;
            
        case SYS_FORK:
        case SYS_EXEC:
            return_value = SYSCALL_ERROR;
            break;
            
        default:
            hal_uart_puts("[SYSCALL] Invalid syscall number\n");
            return_value = SYSCALL_ERROR;
            break;
    }
    
    return return_value;
}
