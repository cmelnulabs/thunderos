.section .text
.global _start

_start:
    # Initialize stack pointer
    lui sp, %hi(0x40000000)
    
    # Initialize global pointer
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    
    # First syscall - write "A\n"
    lui a2, %hi(msg1)
    addi a2, a2, %lo(msg1)
    li a3, 2
    li a1, 1
    li a0, 1
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    ecall
    
    # Second syscall - write "B\n"
    lui a2, %hi(msg2)
    addi a2, a2, %lo(msg2)
    li a3, 2
    li a1, 1
    li a0, 1
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    ecall
    
    # Loop forever
loop:
    j loop

.section .rodata
msg1:
    .string "A\n"
msg2:
    .string "B\n"
