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
    # Swap sp and sscratch
    # If coming from user mode: sscratch has kernel stack, sp has user stack
    # If coming from kernel mode: sscratch is 0, sp stays the same
    csrrw sp, sscratch, sp
    
    # Check if we came from user mode or kernel mode
    # If sscratch (now containing old sp) is 0, we came from kernel
    beqz sp, trap_from_kernel
    
trap_from_user:
    # Coming from user mode
    # sp now points to kernel stack
    # sscratch contains user stack pointer
    
    # Make room on kernel stack for trap frame
    addi sp, sp, -272
    
    # Save user stack pointer (now in sscratch)
    csrr t0, sscratch
    sd t0, 16(sp)     # Save user sp at offset 16 (sp field in trap_frame)
    
    # Continue with saving registers
    j save_registers
    
trap_from_kernel:
    # Coming from kernel mode
    # sscratch was 0, so we swapped sp with 0
    # Restore sp from sscratch
    csrrw sp, sscratch, sp
    
    # Make room on kernel stack for trap frame
    addi sp, sp, -272
    
    # Save kernel stack pointer (before we decremented it)
    addi t0, sp, 272
    sd t0, 16(sp)     # Save kernel sp at offset 16
    
save_registers:
    # Save sepc first (pc field at offset 248)
    csrr t0, sepc
    sd t0, 248(sp)
    
    # Save all general-purpose registers
    # Note: sp already saved above (offset 8)
    sd ra, 0(sp)
    # sp at 8 - already saved
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
    
    # Save sstatus
    csrr t0, sstatus
    sd t0, 256(sp)
    
    # Call C trap handler with trap frame pointer as argument
    mv a0, sp
    call trap_handler
    
    # Check if we need to switch to user mode
    # If trap frame's sstatus has SPP=0, we're returning to user mode
    ld t0, 256(sp)
    andi t1, t0, (1 << 8)  # Check SPP bit (bit 8)
    beqz t1, restore_to_user
    
restore_to_kernel:
    # Returning to kernel mode
    # Restore all registers
    
    # Restore sepc and sstatus
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
    ld s3, 152(sp)
    ld s4, 160(sp)
    ld s5, 168(sp)
    ld s6, 176(sp)
    ld s7, 184(sp)
    ld s8, 192(sp)
    ld s9, 200(sp)
    ld s10, 208(sp)
    ld s11, 216(sp)
    ld t3, 224(sp)
    ld t4, 232(sp)
    ld t5, 240(sp)
    ld t6, 240(sp)
    
    # Restore sp last
    ld sp, 8(sp)
    
    # Ensure sscratch is 0 (we're in kernel mode)
    csrw sscratch, zero
    
    # Return from trap
    sret

restore_to_user:
    # Returning to user mode
    # Put kernel stack pointer in sscratch for next trap
    addi t0, sp, 272   # Kernel stack pointer (before trap frame)
    csrw sscratch, t0
    
    # Restore sepc and sstatus
    ld t0, 248(sp)
    csrw sepc, t0
    ld t0, 256(sp)
    csrw sstatus, t0
    
    # Restore all general-purpose registers except sp
    ld ra, 0(sp)
    # sp at 8 - restore last
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
    ld s3, 152(sp)
    ld s4, 160(sp)
    ld s5, 168(sp)
    ld s6, 176(sp)
    ld s7, 184(sp)
    ld s8, 192(sp)
    ld s9, 200(sp)
    ld s10, 208(sp)
    ld s11, 216(sp)
    ld t3, 224(sp)
    ld t4, 232(sp)
    ld t5, 240(sp)
    ld t6, 240(sp)
    
    # Restore user sp last
    ld sp, 8(sp)
    
    # Return to user mode
    sret
