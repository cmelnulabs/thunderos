/*
 * Trap entry and exit for RISC-V
 * This code saves all registers, calls the C trap handler,
 * then restores all registers and returns.
 * 
 * For user mode support:
 * - sscratch holds the kernel stack pointer when in user mode
 * - sscratch is 0 when in kernel mode
 * - On trap entry, we swap sp and sscratch
 * - This gives us the kernel stack if coming from user mode
 */

.section .text
.global trap_vector
.align 4

trap_vector:
    # Atomically swap sp with sscratch
    # User mode: sscratch=kernel_sp, sp=user_sp → after: sp=kernel_sp, sscratch=user_sp
    # Kernel mode: sscratch=0, sp=kernel_sp → after: sp=0, sscratch=kernel_sp
    csrrw sp, sscratch, sp
    
    # Detect trap source by checking if sp is zero
    # Zero indicates we came from kernel mode (sscratch was 0)
    beqz sp, trap_from_kernel
    
trap_from_user:
    # Save user stack pointer (now in sscratch)
    # Kernel stack is in sp, user stack is in sscratch
    csrr t0, sscratch
    
    # Allocate trap frame on kernel stack
    addi sp, sp, -272
    
    # Save user sp at trap_frame offset 8
    sd t0, 8(sp)
    
    # Continue with saving registers
    j save_registers
    
trap_from_kernel:
    # sscratch was 0, so stack swap left sp=0
    # Restore sp from sscratch and continue
    csrrw sp, sscratch, sp
    
    # Allocate trap frame on kernel stack
    addi sp, sp, -272
    
    # Save kernel sp (before we decremented it)
    addi t0, sp, 272
    sd t0, 8(sp)
    
save_registers:
    # Save sepc (exception program counter)
    csrr t0, sepc
    sd t0, 248(sp)
    
    # Save all general-purpose registers
    # Trap frame layout: 8 bytes per register at offsets 0, 8, 16, ..., 256
    sd ra, 0(sp)
    # sp at offset 8 - already saved above
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)
    
    # Save sstatus (supervisor status register)
    csrr t0, sstatus
    sd t0, 256(sp)
    
    # Call C trap handler with trap_frame pointer as argument
    mv a0, sp
    call trap_handler
    
    # Load sstatus to check return privilege level (SPP bit at position 8)
    # SPP=1 means return to supervisor mode
    # SPP=0 means return to user mode
    ld t0, 256(sp)
    andi t1, t0, (1 << 8)
    beqz t1, restore_to_user
    
restore_to_kernel:
    # Returning to supervisor mode - restore all state from trap frame
    # Restore exception program counter and status
    ld t0, 248(sp)
    csrw sepc, t0
    ld t0, 256(sp)
    csrw sstatus, t0
    
    # Restore general-purpose registers
    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)
    
    # Restore sp last to avoid losing stack pointer during restores
    ld sp, 8(sp)
    
    # Ensure sscratch is zero (kernel mode indicator for next trap)
    csrw sscratch, zero
    
    # Return from exception to supervisor mode
    sret

restore_to_user:
    # Returning to user mode - setup stack swap and restore state
    # Put kernel stack pointer in sscratch for next trap entry
    addi t0, sp, 272
    csrw sscratch, t0
    
    # Restore exception program counter and status for user mode
    ld t0, 248(sp)
    csrw sepc, t0
    ld t0, 256(sp)
    csrw sstatus, t0
    
    # Restore general-purpose registers
    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)
    
    # Restore user stack pointer last
    ld sp, 8(sp)
    
    # Return from exception to user mode (sret restores privilege from sstatus.SPP)
    sret
