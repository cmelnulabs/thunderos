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
    # Save old context if old != NULL
    beqz a0, load_new
    
    # Save callee-saved registers to old context
    sd ra, 0(a0)      # Return address
    sd sp, 8(a0)      # Stack pointer
    sd s0, 16(a0)     # s0-s11
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)
    
load_new:
    # Load new context from a1
    ld ra, 0(a1)      # Return address
    ld sp, 8(a1)      # Stack pointer
    ld s0, 16(a1)     # s0-s11
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)
    
    # Return to new context
    ret

/*
 * process_entry
 * 
 * Entry point for new processes. This is where new processes start executing.
 * It "returns" from a fake trap frame to start the process.
 */
process_entry:
    # When we get here, we're in the new process context
    # We need to load the trap frame and jump to the process entry point
    
    # The trap frame pointer should be in s0 (set up during process creation)
    # For now, we'll call a C function to get it
    call process_get_current_trap_frame
    
    # a0 now contains trap frame pointer
    beqz a0, process_entry_error
    
    mv sp, a0
    
    # Load sepc and sstatus
    ld t0, 248(sp)    # sepc
    csrw sepc, t0
    ld t0, 256(sp)    # sstatus  
    csrw sstatus, t0
    
    # Restore general purpose registers
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
    
    # Load sp into temporary register first, then restore sp last
    ld t0, 8(sp)
    mv sp, t0
    
    # Jump to process (in supervisor mode, not user mode for now)
    sret

process_entry_error:
    # If we couldn't get trap frame, infinite loop
    j process_entry_error
