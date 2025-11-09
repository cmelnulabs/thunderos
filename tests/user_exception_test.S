/*
 * User exception test program (assembly version)
 * Tests user-mode exception handling by causing a page fault
 */

.section .user_exception_test
.global user_exception_start
user_exception_start:
    # Test 1: NULL pointer dereference (should cause load page fault)
    li a0, 0           # NULL pointer
    lw a1, 0(a0)       # Load from NULL - should cause exception
    
    # If we reach here, exception wasn't handled
    li a7, 1           # SYS_EXIT
    li a0, 1           # Exit code 1 (error)
    ecall
    
    # Should not reach here
    j user_exception_start