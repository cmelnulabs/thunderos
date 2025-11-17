/*
 * M-mode entry point for ThunderOS
 * This is the very first code that runs when QEMU starts the kernel
 * 
 * QEMU with -bios none starts execution at 0x80000000 in M-mode
 * We set up a stack and jump to start() in start.c
 */

    .section .text.entry
    .global _entry
_entry:
    # QEMU starts all harts (hardware threads)
    # We only want to run on hart 0, others should spin
    
    # Read hartid from mhartid CSR
    csrr a0, mhartid
    bnez a0, spin          # If not hart 0, go to spin loop
    
    # Hart 0: Set up stack for M-mode
    # Stack grows downward, so set sp to top of stack
    la sp, stack0
    li a0, 4096 * 4        # 4 pages = 16KB stack
    add sp, sp, a0         # sp = stack0 + 16KB
    
    # Call start() in start.c (M-mode initialization)
    call start
    
    # start() should never return (it does mret to S-mode)
    # But just in case, spin forever
    j spin

spin:
    # Other harts and error cases: spin forever
    wfi                    # Wait for interrupt
    j spin

    # Align stack to 16-byte boundary (required by RISC-V ABI)
    .align 4
    .global stack0
stack0:
    # Reserve 16KB for M-mode stack (4 pages × 4KB)
    .space 4096 * 4
