/*
 * System call wrappers in pure assembly
 * This avoids any GCC inline assembly issues with -O0
 */

.section .text

.global syscall0
.global syscall1
.global syscall2
.global syscall3

/* long syscall0(long n) */
syscall0:
    mv a7, a0           # Move syscall number to a7
    ecall              # Make the system call
    ret                # Return with result in a0

/* long syscall1(long n, long a0) */
syscall1:
    mv a7, a0           # Move syscall number to a7
    mv a0, a1           # Move first arg to a0
    ecall              # Make the system call
    ret                # Return with result in a0

/* long syscall2(long n, long a0, long a1) */
syscall2:
    mv a7, a0           # Move syscall number to a7
    mv a0, a1           # Move first arg to a0
    mv a1, a2           # Move second arg to a1
    ecall              # Make the system call
    ret                # Return with result in a0

/* long syscall3(long n, long a0, long a1, long a2) */
syscall3:
    mv a7, a0           # Move syscall number to a7
    mv a0, a1           # Move first arg to a0
    mv a1, a2           # Move second arg to a1
    mv a2, a3           # Move third arg to a2
    ecall              # Make the system call
    ret                # Return with result in a0
