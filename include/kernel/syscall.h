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
#define SYS_GETPPID     10  // Get parent process ID
#define SYS_KILL        11  // Send signal to process
#define SYS_GETTIME     12  // Get system time (milliseconds since boot)
#define SYS_OPEN        13  // Open file
#define SYS_CLOSE       14  // Close file descriptor
#define SYS_LSEEK       15  // Seek file position
#define SYS_STAT        16  // Get file status
#define SYS_MKDIR       17  // Create directory
#define SYS_UNLINK      18  // Remove file
#define SYS_RMDIR       19  // Remove directory
#define SYS_EXECVE      20  // Execute program from file
#define SYS_SIGNAL      21  // Set signal handler
#define SYS_SIGACTION   22  // Advanced signal handling
#define SYS_SIGRETURN   23  // Return from signal handler
#define SYS_MMAP        24  // Map memory
#define SYS_MUNMAP      25  // Unmap memory

#define SYSCALL_COUNT   26

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
uint64_t sys_waitpid(int pid, int *wstatus, int options);
uint64_t sys_write(int fd, const char *buf, size_t len);
uint64_t sys_read(int fd, char *buf, size_t len);
uint64_t sys_getpid(void);
uint64_t sys_sbrk(int increment);
uint64_t sys_sleep(uint64_t ms);
uint64_t sys_yield(void);
uint64_t sys_getppid(void);
uint64_t sys_kill(int pid, int signal);
uint64_t sys_gettime(void);
uint64_t sys_open(const char *path, int flags, int mode);
uint64_t sys_close(int fd);
uint64_t sys_lseek(int fd, int64_t offset, int whence);
uint64_t sys_stat(const char *path, void *statbuf);
uint64_t sys_mkdir(const char *path, int mode);
uint64_t sys_unlink(const char *path);
uint64_t sys_rmdir(const char *path);
uint64_t sys_execve(const char *path, const char *argv[], const char *envp[]);
uint64_t sys_signal(int signum, void (*handler)(int));
uint64_t sys_sigaction(int signum, const void *act, void *oldact);
uint64_t sys_sigreturn(void);
uint64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset);
uint64_t sys_munmap(void *addr, size_t length);

#endif // SYSCALL_H
