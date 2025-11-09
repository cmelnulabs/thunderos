/*
 * User Mode Return Assembly
 * 
 * This file contains the assembly code to return to user mode.
 * It must be in pure assembly (not inline) to avoid compiler interference
 * with register values and stack manipulation.
 */

.section .text
.align 4
.global user_return
.type user_return, @function

/*
 * user_return - Return to user mode
 * 
 * This function restores all registers from a trap frame and executes
 * sret to enter user mode. It MUST be in pure assembly because:
 * 1. The compiler generates prologue/epilogue code that corrupts registers
 * 2. Register clobber lists don't prevent all compiler interference
 * 3. Stack manipulation by the compiler can corrupt saved values
 * 
 * Arguments:
 *   a0 = Pointer to trap_frame structure
 * 
 * The trap_frame layout is (see include/trap.h):
 *   offset 0: ra
 *   offset 8: sp
 *   offset 16: gp
 *   offset 24: tp
 *   offset 32: t0
 *   offset 40: t1
 *   offset 48: t2
 *   offset 56: s0/fp
 *   offset 64: s1
 *   offset 72: a0
 *   offset 80: a1
 *   offset 88: a2
 *   offset 96: a3
 *   offset 104: a4
 *   offset 112: a5
 *   offset 120: a6
 *   offset 128: a7
 *   offset 136: s2
 *   offset 144: s3
 *   offset 152: s4
 *   offset 160: s5
 *   offset 168: s6
 *   offset 176: s7
 *   offset 184: s8
 *   offset 192: s9
 *   offset 200: s10
 *   offset 208: s11
 *   offset 216: t3
 *   offset 224: t4
 *   offset 232: t5
 *   offset 240: t6
 *   offset 248: sepc (PC)
 *   offset 256: sstatus
 */
user_return:
    # a0 points to trap_frame
    mv t6, a0           # Save trap_frame pointer in t6
    
    # Restore sepc (PC to return to)
    ld t0, 248(t6)      # Load sepc
    csrw sepc, t0
    
    # Restore sstatus
    ld t0, 256(t6)      # Load sstatus
    csrw sstatus, t0
    
    # Restore all general-purpose registers except a0 and t6 (we need them)
    ld ra, 0(t6)
    ld sp, 8(t6)
    ld gp, 16(t6)
    ld tp, 24(t6)
    ld t0, 32(t6)
    ld t1, 40(t6)
    ld t2, 48(t6)
    ld s0, 56(t6)
    ld s1, 64(t6)
    # a0 at 72(t6) - restore last
    ld a1, 80(t6)
    ld a2, 88(t6)
    ld a3, 96(t6)
    ld a4, 104(t6)
    ld a5, 112(t6)
    ld a6, 120(t6)
    ld a7, 128(t6)
    ld s2, 136(t6)
    ld s3, 144(t6)
    ld s4, 152(t6)
    ld s5, 160(t6)
    ld s6, 168(t6)
    ld s7, 176(t6)
    ld s8, 184(t6)
    ld s9, 192(t6)
    ld s10, 200(t6)
    ld s11, 208(t6)
    ld t3, 216(t6)
    ld t4, 224(t6)
    ld t5, 232(t6)
    # t6 at 240(t6) - restore last
    
    # Now restore a0 and t6
    ld a0, 72(t6)       # Restore a0
    ld t6, 240(t6)      # Restore t6
    
    # Return to user mode
    # sret will:
    # 1. Set privilege level to SPP field in sstatus (0 = user mode)
    # 2. Jump to address in sepc register
    # 3. Restore interrupt enable from SPIE field in sstatus
    sret

.size user_return, .-user_return
