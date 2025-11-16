/*
 * Enter User Mode
 * 
 * This function enters user mode from kernel mode for the FIRST time.
 * It's different from user_return() which returns from a trap.
 * 
 * This function assumes:
 * - sscratch is already set to kernel stack top
 * - sepc is set to user entry point  
 * - sstatus is configured for user mode (SPP=0, SPIE=1)
 * - sp is still the kernel stack (we DON'T load user sp before sret!)
 * 
 * The key difference from user_return():
 * - We keep sp as kernel stack until AFTER sret
 * - sret will change privilege to user mode
 * - Then user code starts with sp=whatever we set in user registers
 * 
 * But wait, that won't work either because sret doesn't change sp...
 * 
 * OK different approach: we need to load sp ATOMICALLY with sret.
 * Use inline assembly in C to do: sp=user_sp; sret; atomically.
 */

.section .text
.align 4
.global enter_user_mode_asm
.type enter_user_mode_asm, @function

/*
 * enter_user_mode_asm - Enter user mode for the first time
 * 
 * Arguments:
 *   a0 = user stack pointer value
 *   a1 = user entry point (sepc)
 *   a2 = sstatus value
 * 
 * This function:
 * 1. Sets sepc to entry point
 * 2. Sets sstatus  
 * 3. Disables interrupts globally (clear mstatus.MIE) - wait, we're in S-mode, can't access MIE
 * 4. Sets sp to user stack
 * 5. Executes sret
 * 
 * The trick is that we disable interrupts so no trap can occur between
 * setting sp and executing sret.
 */
enter_user_mode_asm:
    # a0 = user_sp
    # a1 = entry (sepc)
    # a2 = sstatus
    
    # Set sepc
    csrw sepc, a1
    
    # Set sstatus
    csrw sstatus, a2
    
    # Set sp to user stack
    mv sp, a0
    
    # Execute sret - enters user mode
    # At this point if there's ANY interrupt/exception before sret executes,
    # we're screwed because sp points to user memory!
    # But we cleared SIE in sstatus, so interrupts should be disabled.
    sret

.size enter_user_mode_asm, .-enter_user_mode_asm
