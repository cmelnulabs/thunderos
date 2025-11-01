/*
 * Context Switch for RISC-V
 * 
 * Saves the current context and restores the next context.
 * This is the low-level mechanism for switching between processes.
 */

.section .text
.global context_switch_asm
.global process_entry

/*
 * void context_switch_asm(struct context *old, struct context *new)
 * 
 * Saves current context to *old (if not NULL) and loads context from *new.
 * 
 * a0 = old context pointer (can be NULL)
 * a1 = new context pointer (must not be NULL)
 */
context_switch_asm:
    # ========================================
    # Save old context (if old != NULL)
    # ========================================
    beqz a0, load_new
    
    # Save callee-saved registers to old context
    sd ra, 0(a0)      # Return address
    sd sp, 8(a0)      # Stack pointer
    sd s0, 16(a0)     # Saved register s0
    sd s1, 24(a0)     # Saved register s1
    sd s2, 32(a0)     # Saved register s2
    sd s3, 40(a0)     # Saved register s3
    sd s4, 48(a0)     # Saved register s4
    sd s5, 56(a0)     # Saved register s5
    sd s6, 64(a0)     # Saved register s6
    sd s7, 72(a0)     # Saved register s7
    sd s8, 80(a0)     # Saved register s8
    sd s9, 88(a0)     # Saved register s9
    sd s10, 96(a0)    # Saved register s10
    sd s11, 104(a0)   # Saved register s11
    
load_new:
    # ========================================
    # Load new context from a1
    # ========================================
    ld ra, 0(a1)      # Return address
    ld sp, 8(a1)      # Stack pointer
    ld s0, 16(a1)     # Saved register s0
    ld s1, 24(a1)     # Saved register s1
    ld s2, 32(a1)     # Saved register s2
    ld s3, 40(a1)     # Saved register s3
    ld s4, 48(a1)     # Saved register s4
    ld s5, 56(a1)     # Saved register s5
    ld s6, 64(a1)     # Saved register s6
    ld s7, 72(a1)     # Saved register s7
    ld s8, 80(a1)     # Saved register s8
    ld s9, 88(a1)     # Saved register s9
    ld s10, 96(a1)    # Saved register s10
    ld s11, 104(a1)   # Saved register s11
    
    # ========================================
    # Return to new context
    # ========================================
    ret

/*
 * process_entry
 * 
 * Entry point for new processes. This is where new processes start executing.
 * It "returns" from a fake trap frame to start the process.
 */
process_entry:
    # ========================================
    # Get trap frame pointer for current process
    # ========================================
    call process_get_current_trap_frame
    
    # Check if trap frame is valid
    beqz a0, process_entry_error
    
    # Use trap frame as stack pointer for loading registers
    mv sp, a0
    
    # ========================================
    # Load processor control registers
    # ========================================
    ld t0, 248(sp)    # Load sepc (exception program counter)
    csrw sepc, t0
    ld t0, 256(sp)    # Load sstatus (supervisor status)
    csrw sstatus, t0
    
    # ========================================
    # Restore general purpose registers
    # ========================================
    ld ra, 0(sp)      # Return address
    ld gp, 16(sp)     # Global pointer
    ld tp, 24(sp)     # Thread pointer
    ld t0, 32(sp)     # Temporary register t0
    ld t1, 40(sp)     # Temporary register t1
    ld t2, 48(sp)     # Temporary register t2
    ld s0, 56(sp)     # Saved register s0
    ld s1, 64(sp)     # Saved register s1
    ld a0, 72(sp)     # Argument register a0
    ld a1, 80(sp)     # Argument register a1
    ld a2, 88(sp)     # Argument register a2
    ld a3, 96(sp)     # Argument register a3
    ld a4, 104(sp)    # Argument register a4
    ld a5, 112(sp)    # Argument register a5
    ld a6, 120(sp)    # Argument register a6
    ld a7, 128(sp)    # Argument register a7
    ld s2, 136(sp)    # Saved register s2
    ld s3, 144(sp)    # Saved register s3
    ld s4, 152(sp)    # Saved register s4
    ld s5, 160(sp)    # Saved register s5
    ld s6, 168(sp)    # Saved register s6
    ld s7, 176(sp)    # Saved register s7
    ld s8, 184(sp)    # Saved register s8
    ld s9, 192(sp)    # Saved register s9
    ld s10, 200(sp)   # Saved register s10
    ld s11, 208(sp)   # Saved register s11
    ld t3, 216(sp)    # Temporary register t3
    ld t4, 224(sp)    # Temporary register t4
    ld t5, 232(sp)    # Temporary register t5
    ld t6, 240(sp)    # Temporary register t6
    
    # ========================================
    # Restore stack pointer
    # ========================================
    # Load sp into temporary register first to avoid corrupting reads
    ld t0, 8(sp)      # Load new sp value
    mv sp, t0         # Restore sp register
    
    # ========================================
    # Jump to process in supervisor mode
    # ========================================
    sret

process_entry_error:
    # Infinite loop if trap frame couldn't be retrieved
    j process_entry_error
