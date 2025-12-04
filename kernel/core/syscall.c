/*
 * System Call Handler for ThunderOS
 * 
 * Implements the system call interface for user-mode processes.
 */

#include "kernel/syscall.h"
#include "kernel/errno.h"
#include "kernel/mutex.h"
#include "kernel/condvar.h"
#include "kernel/rwlock.h"
#include "hal/hal_timer.h"
#include "mm/paging.h"
#include "kernel/process.h"
#include "hal/hal_uart.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include "kernel/elf_loader.h"
#include "drivers/vterm.h"
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
    
    // Find zombie or stopped child process
    while (1) {
        struct process *child = NULL;
        int found_child = 0;
        
        // First check for zombie children (exited)
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
        
        // Check for stopped children (Ctrl+Z)
        extern struct process *process_find_stopped_child(struct process *parent, int target_pid);
        child = process_find_stopped_child(current, pid);
        
        if (child) {
            // Found stopped child - report it but don't reap
            pid_t child_pid = child->pid;
            
            // Store stop status if requested (signal << 8 | 0x7f)
            if (wstatus) {
                *wstatus = child->exit_code;  // Already set by signal_default_stop
            }
            
            // Clear the stopped status so we don't report it again
            // (Process stays stopped until SIGCONT)
            child->exit_code = 0;
            
            return child_pid;
        }
        
        // Check if parent has any children at all
        extern int process_has_children(struct process *parent, int target_pid);
        found_child = process_has_children(current, pid);
        
        if (!found_child) {
            // No such child process
            return SYSCALL_ERROR;
        }
        
        // Child exists but hasn't exited/stopped yet - sleep and try again
        current->state = PROC_SLEEPING;
        scheduler_yield();
        current->state = PROC_RUNNING;
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
 * Timer ticks are every 100ms (TIMER_INTERVAL_US = 100000).
 * 
 * @param milliseconds Milliseconds to sleep
 * @return 0 on success
 */
uint64_t sys_sleep(uint64_t milliseconds) {
    if (milliseconds == 0) {
        return SYSCALL_SUCCESS;
    }
    
    /* Get current time */
    extern uint64_t hal_timer_get_ticks(void);
    uint64_t start_ticks = hal_timer_get_ticks();
    
    /* Each tick is 100ms = 100 milliseconds
     * So to sleep N ms, we need N/100 ticks
     * But to avoid losing precision, round up: (N + 99) / 100
     */
    uint64_t ticks_to_wait = (milliseconds + 99) / 100;
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    uint64_t target_ticks = start_ticks + ticks_to_wait;
    
    /* Enable interrupts so timer interrupts can fire during sleep.
     * 
     * When we enter a syscall via trap, interrupts are disabled by hardware.
     * We need to re-enable them so the timer can update the tick count.
     * 
     * CRITICAL: Before enabling interrupts, we must clear sscratch.
     * The trap entry uses sscratch to detect user vs kernel mode:
     * - sscratch == 0 means we were in kernel mode
     * - sscratch != 0 means we were in user mode (contains kernel stack)
     * If we don't clear it, the trap handler will think we came from user
     * mode and corrupt the stack.
     * 
     * Save sscratch first, then clear it, enable interrupts.
     */
    uint64_t saved_sscratch;
    __asm__ volatile("csrrw %0, sscratch, zero" : "=r"(saved_sscratch));
    
    /* Enable SIE (Supervisor Interrupt Enable) bit in sstatus. */
    __asm__ volatile("csrs sstatus, %0" :: "r"(1UL << 1));
    
    /* Wait until enough ticks have passed.
     * Use WFI to wait for next interrupt instead of spinning.
     */
    while (hal_timer_get_ticks() < target_ticks) {
        __asm__ volatile("wfi");
    }
    
    /* Disable interrupts before restoring sscratch */
    __asm__ volatile("csrc sstatus, %0" :: "r"(1UL << 1));
    
    /* Restore sscratch for proper return to user mode */
    __asm__ volatile("csrw sscratch, %0" :: "r"(saved_sscratch));
    
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
 * @param signal Signal number to send
 * @return 0 on success, -1 on error
 */
uint64_t sys_kill(int pid, int signal) {
    if (pid <= 0) {
        return SYSCALL_ERROR;
    }
    
    /* Find target process */
    extern struct process *process_get(int pid);
    struct process *target = process_get(pid);
    if (!target) {
        return SYSCALL_ERROR;  /* No such process */
    }
    
    /* Send signal to target process */
    extern int signal_send(struct process *proc, int signum);
    int result = signal_send(target, signal);
    
    return (result == 0) ? 0 : SYSCALL_ERROR;
}

/**
 * sys_gettime - Get system time
 * 
 * Timer ticks are every 100ms (TIMER_INTERVAL_US = 100000).
 * 
 * @return Milliseconds since boot
 */
uint64_t sys_gettime(void) {
    // Get time from hardware timer (each tick is 100ms)
    extern uint64_t hal_timer_get_ticks(void);
    uint64_t ticks = hal_timer_get_ticks();
    
    // Convert ticks to milliseconds (each tick = 100ms)
    return ticks * 100;
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
        // Read from input buffer or UART
        if (byte_count == 0) {
            return 0;
        }
        
        // Get this process's controlling terminal
        int tty = process_get_tty(proc);
        
        // If vterm is available, use per-terminal input buffers
        if (vterm_available() && tty >= 0) {
            // Loop until we get input
            while (1) {
                // First check buffer (filled by timer interrupt)
                if (vterm_has_buffered_input_for(tty)) {
                    int buffered = vterm_get_buffered_input_for(tty);
                    if (buffered >= 0) {
                        buffer[0] = (char)buffered;
                        return 1;
                    }
                }
                
                // If we're the active terminal, also check UART directly
                // This provides immediate response without waiting for timer
                // Disable interrupts briefly to avoid race with timer polling
                if (tty == vterm_get_active_index() && hal_uart_data_available()) {
                    // Disable interrupts to prevent timer from also reading UART
                    int old_state = interrupt_save_disable();
                    
                    // Double-check UART still has data after disabling interrupts
                    if (hal_uart_data_available()) {
                        int c = hal_uart_getc_nonblock();
                        if (c >= 0) {
                            // Process through VT system for escape sequences
                            char result = vterm_process_input((char)c);
                            interrupt_restore(old_state);
                            if (result != 0) {
                                buffer[0] = result;
                                return 1;
                            }
                            // Character consumed (VT switch), continue loop
                            continue;
                        }
                    }
                    interrupt_restore(old_state);
                }
                
                // Nothing available, yield and try again
                process_yield();
            }
        } else if (vterm_available()) {
            // Fallback for processes without controlling terminal
            while (!vterm_has_buffered_input()) {
                process_yield();
            }
            int buffered = vterm_get_buffered_input();
            if (buffered >= 0) {
                buffer[0] = (char)buffered;
                return 1;
            }
            return 0;
        } else {
            // No vterm - read directly from UART (fallback)
            char c = hal_uart_getc();
            buffer[0] = c;
            return 1;
        }
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
    
    // Handle stdout/stderr with UART (and optional vterm)
    if (file_descriptor == STDOUT_FD || file_descriptor == STDERR_FD) {
        // If virtual terminals are available, write to process's controlling terminal
        if (vterm_available()) {
            int tty = process_get_tty(proc);
            if (tty >= 0) {
                /* Route output to process's controlling terminal */
                for (size_t i = 0; i < byte_count; i++) {
                    vterm_putc_to(tty, buffer[i]);
                }
                /* Only flush if writing to active terminal */
                if (tty == vterm_get_active_index()) {
                    vterm_flush();
                }
            } else {
                /* No controlling terminal, write to active terminal */
                for (size_t i = 0; i < byte_count; i++) {
                    vterm_putc(buffer[i]);
                }
                vterm_flush();
            }
        } else {
            // Fallback to UART only
            int bytes_written = hal_uart_write(buffer, byte_count);
            if (bytes_written != (int)byte_count) {
                return SYSCALL_ERROR;
            }
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
 * @param statbuf Buffer to store stat information (vfs_stat_t struct)
 * @return 0 on success, -1 on error
 */
uint64_t sys_stat(const char *path, void *statbuf) {
    if (!is_valid_user_pointer(path, 1) || !is_valid_user_pointer(statbuf, sizeof(vfs_stat_t))) {
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
    
    vfs_stat_t *stat_data = (vfs_stat_t *)statbuf;
    int result = vfs_stat_full(path, stat_data);
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
 * sys_dup2 - Duplicate a file descriptor
 * 
 * Makes newfd be the copy of oldfd, closing newfd first if necessary.
 * Used for I/O redirection in shells (e.g., redirecting stdin/stdout to pipes).
 * 
 * @param oldfd The file descriptor to duplicate
 * @param newfd The target file descriptor number
 * @return newfd on success, -1 on error
 * 
 * @errno THUNDEROS_EBADF - oldfd is not a valid file descriptor
 * @errno THUNDEROS_EINVAL - newfd is out of range
 */
uint64_t sys_dup2(int oldfd, int newfd) {
    int result = vfs_dup2(oldfd, newfd);
    if (result < 0) {
        return SYSCALL_ERROR;
    }
    return result;
}

/**
 * sys_getuid - Get real user ID
 * 
 * @return Real user ID of current process
 */
uint64_t sys_getuid(void) {
    struct process *proc = process_current();
    if (!proc) {
        return 0;  /* Default to root */
    }
    return proc->uid;
}

/**
 * sys_getgid - Get real group ID
 * 
 * @return Real group ID of current process
 */
uint64_t sys_getgid(void) {
    struct process *proc = process_current();
    if (!proc) {
        return 0;  /* Default to root */
    }
    return proc->gid;
}

/**
 * sys_geteuid - Get effective user ID
 * 
 * @return Effective user ID of current process
 */
uint64_t sys_geteuid(void) {
    struct process *proc = process_current();
    if (!proc) {
        return 0;  /* Default to root */
    }
    return proc->euid;
}

/**
 * sys_getegid - Get effective group ID
 * 
 * @return Effective group ID of current process
 */
uint64_t sys_getegid(void) {
    struct process *proc = process_current();
    if (!proc) {
        return 0;  /* Default to root */
    }
    return proc->egid;
}

/**
 * sys_chmod - Change file permissions
 * 
 * @param path Path to file
 * @param mode New permission bits (e.g., 0755)
 * @return 0 on success, -1 on error
 */
uint64_t sys_chmod(const char *path, uint32_t mode) {
    if (!path) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    int result = vfs_chmod(path, mode);
    if (result < 0) {
        return SYSCALL_ERROR;
    }
    return 0;
}

/**
 * sys_chown - Change file owner and group
 * 
 * @param path Path to file
 * @param uid New owner user ID
 * @param gid New owner group ID
 * @return 0 on success, -1 on error
 */
uint64_t sys_chown(const char *path, uint16_t uid, uint16_t gid) {
    if (!path) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    int result = vfs_chown(path, uid, gid);
    if (result < 0) {
        return SYSCALL_ERROR;
    }
    return 0;
}

/**
 * Directory entry structure for getdents
 * Similar to Linux struct linux_dirent
 */
struct thunderos_dirent {
    uint32_t d_ino;       /* Inode number */
    uint16_t d_reclen;    /* Record length */
    uint8_t  d_type;      /* File type */
    char     d_name[256]; /* File name (null-terminated) */
};

/**
 * sys_getdents - Get directory entries
 * 
 * Reads directory entries from an open directory file descriptor.
 * 
 * @param fd File descriptor of open directory
 * @param dirp Buffer to store directory entries
 * @param count Size of buffer in bytes
 * @return Number of bytes read on success, 0 on end of directory, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid buffer or count
 * @errno THUNDEROS_EBADF - Invalid file descriptor
 * @errno THUNDEROS_ENOTDIR - fd does not refer to a directory
 */
uint64_t sys_getdents(int fd, void *dirp, size_t count) {
    struct process *proc = process_current();
    if (!proc) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    if (!dirp || count < sizeof(struct thunderos_dirent)) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    // Validate user pointer
    if (!process_validate_user_ptr(proc, dirp, count, VM_WRITE)) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    // Get the file from fd
    vfs_file_t *file = vfs_get_file(fd);
    if (!file || !file->node) {
        set_errno(THUNDEROS_EBADF);
        return SYSCALL_ERROR;
    }
    
    vfs_node_t *node = file->node;
    
    // Must be a directory
    if (node->type != VFS_TYPE_DIRECTORY) {
        set_errno(THUNDEROS_ENOTDIR);
        return SYSCALL_ERROR;
    }
    
    // Check for readdir operation
    if (!node->ops || !node->ops->readdir) {
        set_errno(THUNDEROS_EIO);
        return SYSCALL_ERROR;
    }
    
    // Read directory entries starting from current position
    uint8_t *buf = (uint8_t *)dirp;
    size_t bytes_written = 0;
    uint32_t index = file->pos;
    
    char name[256];
    uint32_t inode_num;
    
    while (bytes_written + sizeof(struct thunderos_dirent) <= count) {
        int ret = node->ops->readdir(node, index, name, &inode_num);
        if (ret != 0) {
            // No more entries
            break;
        }
        
        // Create dirent entry
        struct thunderos_dirent *entry = (struct thunderos_dirent *)(buf + bytes_written);
        entry->d_ino = inode_num;
        entry->d_reclen = sizeof(struct thunderos_dirent);
        entry->d_type = 0;  // DT_UNKNOWN for now
        
        // Copy name
        uint32_t name_len = 0;
        while (name[name_len] && name_len < 255) {
            entry->d_name[name_len] = name[name_len];
            name_len++;
        }
        entry->d_name[name_len] = '\0';
        
        bytes_written += sizeof(struct thunderos_dirent);
        index++;
    }
    
    // Update file position
    file->pos = index;
    
    clear_errno();
    return bytes_written;
}

/**
 * sys_chdir - Change current working directory
 * 
 * Supports both absolute and relative paths. The path is normalized
 * before storing in the process control block.
 * 
 * @param path Path to new working directory (absolute or relative)
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid path
 * @errno THUNDEROS_ENOENT - Path does not exist
 * @errno THUNDEROS_ENOTDIR - Path is not a directory
 */
uint64_t sys_chdir(const char *path) {
    struct process *proc = process_current();
    if (!proc) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    if (!is_valid_user_pointer(path, 1)) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    /* Normalize the path (handles relative paths, ., ..) */
    char normalized_path[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized_path, VFS_MAX_PATH) < 0) {
        /* errno already set by vfs_normalize_path */
        return SYSCALL_ERROR;
    }
    
    /* Resolve the normalized path to verify it exists and is a directory */
    vfs_node_t *node = vfs_resolve_path(normalized_path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return SYSCALL_ERROR;
    }
    
    if (node->type != VFS_TYPE_DIRECTORY) {
        set_errno(THUNDEROS_ENOTDIR);
        return SYSCALL_ERROR;
    }
    
    /* Store the normalized absolute path in process cwd */
    size_t path_index = 0;
    while (normalized_path[path_index] && path_index < VFS_MAX_PATH - 1) {
        proc->cwd[path_index] = normalized_path[path_index];
        path_index++;
    }
    proc->cwd[path_index] = '\0';
    
    clear_errno();
    return SYSCALL_SUCCESS;
}

/**
 * sys_getcwd - Get current working directory
 * 
 * @param buf Buffer to store path
 * @param size Size of buffer
 * @return Pointer to buf on success, NULL on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid buffer or size
 * @errno THUNDEROS_ERANGE - Buffer too small for path
 */
uint64_t sys_getcwd(char *buf, size_t size) {
    struct process *proc = process_current();
    if (!proc) {
        set_errno(THUNDEROS_EINVAL);
        return (uint64_t)NULL;
    }
    
    if (!buf || size == 0) {
        set_errno(THUNDEROS_EINVAL);
        return (uint64_t)NULL;
    }
    
    if (!process_validate_user_ptr(proc, buf, size, VM_WRITE)) {
        set_errno(THUNDEROS_EINVAL);
        return (uint64_t)NULL;
    }
    
    // Check if buffer is large enough
    size_t cwd_len = 0;
    while (proc->cwd[cwd_len]) cwd_len++;
    
    if (cwd_len >= size) {
        set_errno(THUNDEROS_ERANGE);
        return (uint64_t)NULL;
    }
    
    // Copy cwd to buffer
    size_t i = 0;
    while (proc->cwd[i] && i < size - 1) {
        buf[i] = proc->cwd[i];
        i++;
    }
    buf[i] = '\0';
    
    clear_errno();
    return (uint64_t)buf;
}

/**
 * sys_setsid - Create a new session
 * 
 * Creates a new session with the calling process as the session leader.
 * The process becomes the controlling process of a new terminal.
 * 
 * @return New session ID (process PID) on success, -1 on error
 */
uint64_t sys_setsid(void) {
    /* Use the proper process group implementation */
    return (uint64_t)process_setsid();
}

/**
 * sys_setpgid - Set process group ID
 * 
 * Sets the process group ID of the specified process.
 * 
 * @param pid Process ID (0 for current process)
 * @param pgid New process group ID (0 to use pid as pgid)
 * @return 0 on success, -1 on error
 */
uint64_t sys_setpgid(int pid, int pgid) {
    return (uint64_t)process_setpgid((pid_t)pid, (pid_t)pgid);
}

/**
 * sys_getpgid - Get process group ID
 * 
 * Returns the process group ID of the specified process.
 * 
 * @param pid Process ID (0 for current process)
 * @return Process group ID, or -1 on error
 */
uint64_t sys_getpgid(int pid) {
    return (uint64_t)process_getpgid((pid_t)pid);
}

/**
 * sys_getsid - Get session ID
 * 
 * Returns the session ID of the specified process.
 * 
 * @param pid Process ID (0 for current process)
 * @return Session ID, or -1 on error
 */
uint64_t sys_getsid(int pid) {
    return (uint64_t)process_getsid((pid_t)pid);
}

/**
 * sys_gettty - Get controlling terminal
 * 
 * Returns the index of the process's controlling terminal.
 * 
 * @return Terminal index (0-5), or -1 if no controlling terminal
 */
uint64_t sys_gettty(void) {
    struct process *proc = process_current();
    if (!proc) {
        return SYSCALL_ERROR;
    }
    
    return (uint64_t)proc->controlling_tty;
}

/**
 * sys_settty - Set controlling terminal
 * 
 * Sets the controlling terminal for the current process.
 * The terminal must be a valid virtual terminal index (0-5).
 * 
 * @param tty Terminal index to set, or -1 to detach
 * @return 0 on success, -1 on error
 */
uint64_t sys_settty(int tty) {
    struct process *proc = process_current();
    if (!proc) {
        return SYSCALL_ERROR;
    }
    
    /* Validate terminal index */
    if (tty < -1 || tty >= 6) {  /* VTERM_MAX_TERMINALS = 6 */
        return SYSCALL_ERROR;
    }
    
    proc->controlling_tty = tty;
    return 0;
}

/**
 * sys_getprocs - Get information about running processes
 * 
 * Fills a user-provided buffer with information about all active processes.
 * This is used by the 'ps' utility to display process status.
 * 
 * @param buf User buffer to store procinfo_t structures
 * @param max_procs Maximum number of processes to return
 * @return Number of processes returned, or -1 on error
 */
uint64_t sys_getprocs(procinfo_t *buf, size_t max_procs) {
    if (!buf || max_procs == 0) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    /* Validate user buffer */
    if (!is_valid_user_pointer(buf, max_procs * sizeof(procinfo_t))) {
        set_errno(THUNDEROS_EFAULT);
        return SYSCALL_ERROR;
    }
    
    int max_count = process_get_max_count();
    size_t count = 0;
    
    for (int i = 0; i < max_count && count < max_procs; i++) {
        struct process *p = process_get_by_index(i);
        if (p != NULL) {
            buf[count].pid = p->pid;
            buf[count].ppid = p->parent ? p->parent->pid : 0;
            buf[count].pgid = p->pgid;
            buf[count].sid = p->sid;
            buf[count].state = p->state;
            buf[count].tty = p->controlling_tty;
            buf[count].cpu_time = p->cpu_time;
            
            /* Copy name safely */
            for (int j = 0; j < PROC_NAME_MAX - 1 && p->name[j]; j++) {
                buf[count].name[j] = p->name[j];
            }
            buf[count].name[PROC_NAME_MAX - 1] = '\0';
            
            count++;
        }
    }
    
    clear_errno();
    return count;
}

/**
 * sys_uname - Get system information
 * 
 * Returns information about the operating system.
 * 
 * @param buf User buffer to store utsname_t structure
 * @return 0 on success, -1 on error
 */
uint64_t sys_uname(utsname_t *buf) {
    if (!buf) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    /* Validate user buffer */
    if (!is_valid_user_pointer(buf, sizeof(utsname_t))) {
        set_errno(THUNDEROS_EFAULT);
        return SYSCALL_ERROR;
    }
    
    /* Helper to copy string safely */
    #define COPY_STR(dst, src) do { \
        int i; \
        for (i = 0; i < 63 && (src)[i]; i++) (dst)[i] = (src)[i]; \
        (dst)[i] = '\0'; \
    } while(0)
    
    COPY_STR(buf->sysname, "ThunderOS");
    COPY_STR(buf->nodename, "thunderos");
    COPY_STR(buf->release, "0.7.0");
    COPY_STR(buf->version, "v0.7.0 Virtual Terminals");
    COPY_STR(buf->machine, "riscv64");
    
    #undef COPY_STR
    
    clear_errno();
    return 0;
}

/* ========================================================================
 * Mutex Syscalls
 * ======================================================================== */

#define MAX_USER_MUTEXES 64

static mutex_t user_mutexes[MAX_USER_MUTEXES];
static int mutex_in_use[MAX_USER_MUTEXES] = {0};

/* ========================================================================
 * Condition Variable Syscalls
 * ======================================================================== */

#define MAX_USER_CONDVARS 64

static condvar_t user_condvars[MAX_USER_CONDVARS];
static int condvar_in_use[MAX_USER_CONDVARS] = {0};

/**
 * sys_mutex_create - Create a new mutex
 * 
 * @return Mutex ID (>= 0) on success, -1 on error
 */
uint64_t sys_mutex_create(void) {
    /* Find a free mutex slot */
    for (int i = 0; i < MAX_USER_MUTEXES; i++) {
        if (!mutex_in_use[i]) {
            mutex_init(&user_mutexes[i]);
            mutex_in_use[i] = 1;
            clear_errno();
            return (uint64_t)i;
        }
    }
    
    set_errno(THUNDEROS_ENOMEM);
    return SYSCALL_ERROR;
}

/**
 * sys_mutex_lock - Lock a mutex (blocking)
 * 
 * @param mutex_id Mutex ID from sys_mutex_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_mutex_lock(int mutex_id) {
    if (mutex_id < 0 || mutex_id >= MAX_USER_MUTEXES || !mutex_in_use[mutex_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    mutex_lock(&user_mutexes[mutex_id]);
    clear_errno();
    return 0;
}

/**
 * sys_mutex_trylock - Try to lock a mutex (non-blocking)
 * 
 * @param mutex_id Mutex ID from sys_mutex_create
 * @return 0 if locked, -1 if already locked or error
 */
uint64_t sys_mutex_trylock(int mutex_id) {
    if (mutex_id < 0 || mutex_id >= MAX_USER_MUTEXES || !mutex_in_use[mutex_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    return mutex_trylock(&user_mutexes[mutex_id]);
}

/**
 * sys_mutex_unlock - Unlock a mutex
 * 
 * @param mutex_id Mutex ID from sys_mutex_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_mutex_unlock(int mutex_id) {
    if (mutex_id < 0 || mutex_id >= MAX_USER_MUTEXES || !mutex_in_use[mutex_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    mutex_unlock(&user_mutexes[mutex_id]);
    clear_errno();
    return 0;
}

/**
 * sys_mutex_destroy - Destroy a mutex
 * 
 * @param mutex_id Mutex ID from sys_mutex_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_mutex_destroy(int mutex_id) {
    if (mutex_id < 0 || mutex_id >= MAX_USER_MUTEXES || !mutex_in_use[mutex_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    mutex_in_use[mutex_id] = 0;
    clear_errno();
    return 0;
}

/**
 * sys_cond_create - Create a new condition variable
 * 
 * @return Condition variable ID (>= 0) on success, -1 on error
 */
uint64_t sys_cond_create(void) {
    /* Find a free condvar slot */
    for (int i = 0; i < MAX_USER_CONDVARS; i++) {
        if (!condvar_in_use[i]) {
            cond_init(&user_condvars[i]);
            condvar_in_use[i] = 1;
            clear_errno();
            return (uint64_t)i;
        }
    }
    
    set_errno(THUNDEROS_ENOMEM);
    return SYSCALL_ERROR;
}

/**
 * sys_cond_wait - Wait on a condition variable (blocking)
 * 
 * Atomically unlocks the mutex and sleeps on the condition variable.
 * When awakened, re-acquires the mutex before returning.
 * 
 * @param cond_id Condition variable ID from sys_cond_create
 * @param mutex_id Mutex ID (must be locked by caller)
 * @return 0 on success, -1 on error
 */
uint64_t sys_cond_wait(int cond_id, int mutex_id) {
    if (cond_id < 0 || cond_id >= MAX_USER_CONDVARS || !condvar_in_use[cond_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    if (mutex_id < 0 || mutex_id >= MAX_USER_MUTEXES || !mutex_in_use[mutex_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    cond_wait(&user_condvars[cond_id], &user_mutexes[mutex_id]);
    clear_errno();
    return 0;
}

/**
 * sys_cond_signal - Signal one waiter on a condition variable
 * 
 * Wakes up one process waiting on the condition variable (if any).
 * 
 * @param cond_id Condition variable ID from sys_cond_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_cond_signal(int cond_id) {
    if (cond_id < 0 || cond_id >= MAX_USER_CONDVARS || !condvar_in_use[cond_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    cond_signal(&user_condvars[cond_id]);
    clear_errno();
    return 0;
}

/**
 * sys_cond_broadcast - Wake all waiters on a condition variable
 * 
 * Wakes up all processes waiting on the condition variable.
 * 
 * @param cond_id Condition variable ID from sys_cond_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_cond_broadcast(int cond_id) {
    if (cond_id < 0 || cond_id >= MAX_USER_CONDVARS || !condvar_in_use[cond_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    cond_broadcast(&user_condvars[cond_id]);
    clear_errno();
    return 0;
}

/**
 * sys_cond_destroy - Destroy a condition variable
 * 
 * @param cond_id Condition variable ID from sys_cond_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_cond_destroy(int cond_id) {
    if (cond_id < 0 || cond_id >= MAX_USER_CONDVARS || !condvar_in_use[cond_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    cond_destroy(&user_condvars[cond_id]);
    condvar_in_use[cond_id] = 0;
    clear_errno();
    return 0;
}

/* ========================================================================
 * Reader-Writer Lock Syscalls
 * ======================================================================== */

#define MAX_USER_RWLOCKS 64

static rwlock_t user_rwlocks[MAX_USER_RWLOCKS];
static int rwlock_in_use[MAX_USER_RWLOCKS] = {0};

/**
 * sys_rwlock_create - Create a new reader-writer lock
 * 
 * @return RWLock ID (>= 0) on success, -1 on error
 */
uint64_t sys_rwlock_create(void) {
    /* Find a free rwlock slot */
    for (int i = 0; i < MAX_USER_RWLOCKS; i++) {
        if (!rwlock_in_use[i]) {
            rwlock_init(&user_rwlocks[i]);
            rwlock_in_use[i] = 1;
            clear_errno();
            return (uint64_t)i;
        }
    }
    
    set_errno(THUNDEROS_ENOMEM);
    return SYSCALL_ERROR;
}

/**
 * sys_rwlock_read_lock - Acquire read lock (blocking)
 * 
 * @param rwlock_id RWLock ID from sys_rwlock_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_rwlock_read_lock(int rwlock_id) {
    if (rwlock_id < 0 || rwlock_id >= MAX_USER_RWLOCKS || !rwlock_in_use[rwlock_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    rwlock_read_lock(&user_rwlocks[rwlock_id]);
    clear_errno();
    return 0;
}

/**
 * sys_rwlock_read_unlock - Release read lock
 * 
 * @param rwlock_id RWLock ID from sys_rwlock_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_rwlock_read_unlock(int rwlock_id) {
    if (rwlock_id < 0 || rwlock_id >= MAX_USER_RWLOCKS || !rwlock_in_use[rwlock_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    rwlock_read_unlock(&user_rwlocks[rwlock_id]);
    clear_errno();
    return 0;
}

/**
 * sys_rwlock_write_lock - Acquire write lock (blocking)
 * 
 * @param rwlock_id RWLock ID from sys_rwlock_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_rwlock_write_lock(int rwlock_id) {
    if (rwlock_id < 0 || rwlock_id >= MAX_USER_RWLOCKS || !rwlock_in_use[rwlock_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    rwlock_write_lock(&user_rwlocks[rwlock_id]);
    clear_errno();
    return 0;
}

/**
 * sys_rwlock_write_unlock - Release write lock
 * 
 * @param rwlock_id RWLock ID from sys_rwlock_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_rwlock_write_unlock(int rwlock_id) {
    if (rwlock_id < 0 || rwlock_id >= MAX_USER_RWLOCKS || !rwlock_in_use[rwlock_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    rwlock_write_unlock(&user_rwlocks[rwlock_id]);
    clear_errno();
    return 0;
}

/**
 * sys_rwlock_destroy - Destroy a reader-writer lock
 * 
 * @param rwlock_id RWLock ID from sys_rwlock_create
 * @return 0 on success, -1 on error
 */
uint64_t sys_rwlock_destroy(int rwlock_id) {
    if (rwlock_id < 0 || rwlock_id >= MAX_USER_RWLOCKS || !rwlock_in_use[rwlock_id]) {
        set_errno(THUNDEROS_EINVAL);
        return SYSCALL_ERROR;
    }
    
    rwlock_in_use[rwlock_id] = 0;
    clear_errno();
    return 0;
}

/**
 * sys_fork - Create a child process
 * 
 * Creates a complete copy of the current process with its own address space.
 * Both processes continue execution after fork() returns, with different return values:
 * - Parent receives child PID
 * - Child receives 0
 * 
 * The child process gets:
 * - Copy of all memory pages (code, data, stack, heap)
 * - Copy of all VMAs (virtual memory areas)
 * - Copy of all open file descriptors
 * - Separate page table (memory isolation)
 * - Same instruction pointer (continues from fork() call)
 * 
 * @return Child PID in parent process, 0 in child process, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - No current process
 * @errno THUNDEROS_EAGAIN - Process table full
 * @errno THUNDEROS_ENOMEM - Out of memory
 * 
 * Example:
 *   pid_t pid = fork();
 *   if (pid < 0) {
 *     // Error
 *   } else if (pid == 0) {
 *     // Child process
 *   } else {
 *     // Parent process (pid = child's PID)
 *   }
 */
uint64_t sys_fork(struct trap_frame *tf) {
    struct process *current = process_current();
    if (!current) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    if (!tf) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Call kernel fork implementation with current trap frame
    // The child will get a copy of this trap frame
    pid_t child_pid = process_fork(tf);
    
    // Return child PID to parent, or -1 on error (errno already set)
    return child_pid;
}

/**
 * sys_execve_with_frame - Execute program from filesystem (with trap frame)
 * 
 * @param tf Trap frame pointer (for updating entry point)
 * @param path Path to executable
 * @param argv Argument array
 * @param envp Environment array (ignored)
 * @return Does not return on success, -1 on error
 */
uint64_t sys_execve_with_frame(struct trap_frame *tf, const char *path, const char *argv[], const char *envp[]) {
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
    
    // Replace current process with new ELF binary
    // On success, this modifies trap_frame to jump to new entry point
    // and returns 0. On error, returns -1.
    int result = elf_exec_replace(path, argv, argc, tf);
    
    if (result == 0) {
        // SUCCESS: exec replaced the process
        // The trap frame has been modified to jump to the new program
        // We must NOT modify trap_frame->a0 - just return 
        // The syscall handler should detect exec success and not modify a0
        return 0;  // Special: tells syscall_handler not to modify a0
    }
    
    // FAILURE: exec failed
    return SYSCALL_ERROR;
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
/**
 * Syscall handler with trap frame for syscalls that need full register state
 */
uint64_t syscall_handler_with_frame(struct trap_frame *tf,
                                    uint64_t syscall_number, 
                                    uint64_t argument0, uint64_t argument1, uint64_t argument2,
                                    uint64_t argument3, uint64_t argument4, uint64_t argument5) {
    // For fork, we need to pass the current trap frame
    if (syscall_number == SYS_FORK) {
        return sys_fork(tf);
    }
    
    // For execve, we need to pass the trap frame to update sepc
    if (syscall_number == SYS_EXECVE) {
        return sys_execve_with_frame(tf, (const char *)argument0, (const char **)argument1, (const char **)argument2);
    }
    
    // For all other syscalls, use the normal handler
    return syscall_handler(syscall_number, argument0, argument1, argument2, 
                          argument3, argument4, argument5);
}

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
            // Execve is handled in syscall_handler_with_frame
            return_value = SYSCALL_ERROR;
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
            
        case SYS_GETDENTS:
            return_value = sys_getdents((int)argument0, (void *)argument1, (size_t)argument2);
            break;
            
        case SYS_CHDIR:
            return_value = sys_chdir((const char *)argument0);
            break;
            
        case SYS_GETCWD:
            return_value = sys_getcwd((char *)argument0, (size_t)argument1);
            break;
            
        case SYS_SETSID:
            return_value = sys_setsid();
            break;
            
        case SYS_GETTTY:
            return_value = sys_gettty();
            break;
            
        case SYS_SETTTY:
            return_value = sys_settty((int)argument0);
            break;
            
        case SYS_GETPROCS:
            return_value = sys_getprocs((procinfo_t *)argument0, (size_t)argument1);
            break;
            
        case SYS_UNAME:
            return_value = sys_uname((utsname_t *)argument0);
            break;
            
        case SYS_DUP2:
            return_value = sys_dup2((int)argument0, (int)argument1);
            break;
            
        case SYS_SETFGPID: {
            /* Set foreground process for current terminal */
            extern void vterm_set_active_fg_pid(int pid);
            int pid = (int)argument0;
            vterm_set_active_fg_pid(pid);
            return_value = 0;
            break;
        }
        
        case SYS_GETUID:
            return_value = sys_getuid();
            break;
            
        case SYS_GETGID:
            return_value = sys_getgid();
            break;
            
        case SYS_GETEUID:
            return_value = sys_geteuid();
            break;
            
        case SYS_GETEGID:
            return_value = sys_getegid();
            break;
            
        case SYS_CHMOD:
            return_value = sys_chmod((const char *)argument0, (uint32_t)argument1);
            break;
            
        case SYS_CHOWN:
            return_value = sys_chown((const char *)argument0, (uint16_t)argument1, (uint16_t)argument2);
            break;
            
        case SYS_SETPGID:
            return_value = sys_setpgid((int)argument0, (int)argument1);
            break;
            
        case SYS_GETPGID:
            return_value = sys_getpgid((int)argument0);
            break;
            
        case SYS_GETSID:
            return_value = sys_getsid((int)argument0);
            break;
            
        case SYS_MUTEX_CREATE:
            return_value = sys_mutex_create();
            break;
            
        case SYS_MUTEX_LOCK:
            return_value = sys_mutex_lock((int)argument0);
            break;
            
        case SYS_MUTEX_TRYLOCK:
            return_value = sys_mutex_trylock((int)argument0);
            break;
            
        case SYS_MUTEX_UNLOCK:
            return_value = sys_mutex_unlock((int)argument0);
            break;
            
        case SYS_MUTEX_DESTROY:
            return_value = sys_mutex_destroy((int)argument0);
            break;
            
        case SYS_COND_CREATE:
            return_value = sys_cond_create();
            break;
            
        case SYS_COND_WAIT:
            return_value = sys_cond_wait((int)argument0, (int)argument1);
            break;
            
        case SYS_COND_SIGNAL:
            return_value = sys_cond_signal((int)argument0);
            break;
            
        case SYS_COND_BROADCAST:
            return_value = sys_cond_broadcast((int)argument0);
            break;
            
        case SYS_COND_DESTROY:
            return_value = sys_cond_destroy((int)argument0);
            break;
            
        case SYS_RWLOCK_CREATE:
            return_value = sys_rwlock_create();
            break;
            
        case SYS_RWLOCK_READ_LOCK:
            return_value = sys_rwlock_read_lock((int)argument0);
            break;
            
        case SYS_RWLOCK_READ_UNLOCK:
            return_value = sys_rwlock_read_unlock((int)argument0);
            break;
            
        case SYS_RWLOCK_WRITE_LOCK:
            return_value = sys_rwlock_write_lock((int)argument0);
            break;
            
        case SYS_RWLOCK_WRITE_UNLOCK:
            return_value = sys_rwlock_write_unlock((int)argument0);
            break;
            
        case SYS_RWLOCK_DESTROY:
            return_value = sys_rwlock_destroy((int)argument0);
            break;
            
        case SYS_FORK:
            // Fork is handled in syscall_handler_with_frame
            // This should not be reached
            hal_uart_puts("[WARN] SYS_FORK called from old syscall_handler\n");
            return_value = SYSCALL_ERROR;
            break;
            break;
            
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
