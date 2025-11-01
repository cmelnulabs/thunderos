/*
 * System Call Handler for ThunderOS
 * 
 * Implements the system call interface for user-mode processes.
 */

#include "kernel/syscall.h"
#include "kernel/process.h"
#include "hal/hal_uart.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include <stdint.h>
#include <stddef.h>

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
 * is_valid_user_pointer - Validate user-space pointer
 * 
 * Performs basic validation to ensure pointer is safe to dereference.
 * Checks for NULL, kernel space addresses, and overflow.
 * 
 * @param pointer User-space pointer to validate
 * @param length Length of memory region in bytes
 * @return 1 if valid, 0 if invalid
 */
static int is_valid_user_pointer(const void *pointer, size_t length) {
    uintptr_t address = (uintptr_t)pointer;
    
    if (pointer == NULL) {
        return 0;
    }
    
    // User addresses must be in lower half (below kernel space)
    if (address >= KERNEL_SPACE_START) {
        return 0;
    }
    
    // Check for address overflow
    uintptr_t end_address = address + length;
    if (end_address < address) {
        return 0;
    }
    
    return 1;
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
 * sys_write - Write data to a file descriptor
 * 
 * @param file_descriptor File descriptor (1 = stdout, 2 = stderr)
 * @param buffer Buffer to write from
 * @param byte_count Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
uint64_t sys_write(int file_descriptor, const char *buffer, size_t byte_count) {
    if (!is_valid_user_pointer(buffer, byte_count)) {
        return SYSCALL_ERROR;
    }
    
    if (file_descriptor != STDOUT_FD && file_descriptor != STDERR_FD) {
        return SYSCALL_ERROR;
    }
    
    // Use batch write for efficiency
    int bytes_written = hal_uart_write(buffer, byte_count);
    if (bytes_written != (int)byte_count) {
        return SYSCALL_ERROR;
    }
    
    return byte_count;
}

/**
 * sys_read - Read data from a file descriptor
 * 
 * @param file_descriptor File descriptor (0 = stdin)
 * @param buffer Buffer to read into
 * @param byte_count Maximum number of bytes to read
 * @return Number of bytes read, or -1 on error
 */
uint64_t sys_read(int file_descriptor, char *buffer, size_t byte_count) {
    if (!is_valid_user_pointer(buffer, byte_count)) {
        return SYSCALL_ERROR;
    }
    
    if (file_descriptor != STDIN_FD) {
        return SYSCALL_ERROR;
    }
    
    // Not implemented yet - requires input buffering
    // Return 0 (EOF) for now
    return 0;
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
 * sys_sbrk - Adjust heap size
 * 
 * @param heap_increment Bytes to add to heap (can be negative)
 * @return Previous heap end, or -1 on error
 */
uint64_t sys_sbrk(int heap_increment) {
    (void)heap_increment;  // Suppress unused parameter warning
    
    // Not implemented yet - requires per-process heap management
    return SYSCALL_ERROR;
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
            
        case SYS_FORK:
        case SYS_EXEC:
        case SYS_WAIT:
            return_value = SYSCALL_ERROR;
            break;
            
        default:
            hal_uart_puts("[SYSCALL] Invalid syscall number\n");
            return_value = SYSCALL_ERROR;
            break;
    }
    
    return return_value;
}
