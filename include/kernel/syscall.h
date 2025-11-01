#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// System call numbers
#define SYS_EXIT        0   // Exit process
#define SYS_WRITE       1   // Write to file descriptor
#define SYS_READ        2   // Read from file descriptor
#define SYS_GETPID      3   // Get process ID
#define SYS_SBRK        4   // Adjust heap size
#define SYS_SLEEP       5   // Sleep for milliseconds
#define SYS_YIELD       6   // Yield CPU to scheduler
#define SYS_FORK        7   // Fork process (future)
#define SYS_EXEC        8   // Execute program (future)
#define SYS_WAIT        9   // Wait for child (future)

#define SYSCALL_COUNT   10

// RISC-V Syscall ABI:
// - Syscall number in a7 (x17)
// - Arguments in a0-a5 (x10-x15)
// - Return value in a0 (x10)
// - Uses ECALL instruction from user mode

/**
 * Main syscall handler
 * Called from trap handler when ECALL is executed from user mode
 * 
 * @param syscall_num Syscall number (from a7)
 * @param arg0-arg5 Syscall arguments (from a0-a5)
 * @return Return value (placed in a0)
 */
uint64_t syscall_handler(uint64_t syscall_num, 
                        uint64_t arg0, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5);

// Individual syscall implementations
uint64_t sys_exit(int status);
uint64_t sys_write(int fd, const char *buf, size_t len);
uint64_t sys_read(int fd, char *buf, size_t len);
uint64_t sys_getpid(void);
uint64_t sys_sbrk(int increment);
uint64_t sys_sleep(uint64_t ms);
uint64_t sys_yield(void);

#endif // SYSCALL_H
