/* 
 * ThunderOS - S-mode Boot Initialization
 * RISC-V 64-bit assembly
 *
 * This code runs in Supervisor mode after M-mode initialization completes.
 * It sets up the minimal S-mode environment needed to run C code, then
 * jumps to kernel_main().
 *
 * Execution flow (with -bios none):
 *   1. QEMU starts at 0x80000000 → entry.S (_entry, M-mode)
 *   2. entry.S calls start() in start.c (M-mode CSR configuration)
 *   3. start() does mret → _start (this file, S-mode)
 *   4. We setup S-mode stack, clear BSS, jump to kernel_main()
 */

.section .text.boot          # Place code in the .text.boot section (linked first)
.global _start               # Make _start visible to linker (entry point)

_start:
    # ========================================================================
    # Step 1: Disable interrupts during initialization
    # ========================================================================
    # We don't want interrupts firing while we're setting up the environment.
    # The SIE (Supervisor Interrupt Enable) register controls which interrupts
    # are enabled. Writing zero disables all interrupts.
    csrw sie, zero           # CSR Write: sie = 0 (disable all S-mode interrupts)
    
    # ========================================================================
    # Step 2: Setup the stack pointer
    # ========================================================================
    # C code needs a valid stack to work. The stack grows downward in memory,
    # so we point SP to the TOP of our reserved stack space.
    # _stack_top is defined at the bottom of this file.
    la sp, _stack_top        # Load Address: sp = address of _stack_top
    
    # ========================================================================
    # Step 3: Clear the BSS section
    # ========================================================================
    # BSS (Block Started by Symbol) contains uninitialized global/static variables.
    # The C standard requires these to be zero-initialized before main() runs.
    # _bss_start and _bss_end are defined by the linker script.
    la t0, _bss_start        # t0 = start address of BSS
    la t1, _bss_end          # t1 = end address of BSS
clear_bss:
    beq t0, t1, clear_bss_done   # If t0 == t1, we're done (empty or finished)
    sd zero, 0(t0)               # Store Doubleword: write 8 bytes of zero at *t0
    addi t0, t0, 8               # Add Immediate: t0 += 8 (move to next 8-byte slot)
    j clear_bss                  # Jump: repeat the loop
clear_bss_done:
    
    # ========================================================================
    # Step 4: Jump to C kernel entry point
    # ========================================================================
    # Environment is ready! Transfer control to kernel_main() in kernel/main.c
    # The 'call' instruction saves return address in ra register, but
    # kernel_main() should never return.
    call kernel_main         # Call function (ra = PC+4, PC = kernel_main)
    
    # ========================================================================
    # Step 5: Halt if kernel_main returns (should never happen)
    # ========================================================================
    # If we reach here, something went wrong. Enter an infinite low-power loop.
halt:
    wfi                      # Wait For Interrupt (low-power idle state)
    j halt                   # Jump: infinite loop (in case interrupt wakes us)

# ============================================================================
# Stack definition
# ============================================================================
# The stack is placed in the BSS section (uninitialized data).
# We reserve 16KB (4 pages) for the boot stack.
# Note: stack grows DOWNWARD, so _stack_top is at the END of the reserved space.

.section .bss                # Uninitialized data section
.align 12                    # Align to 2^12 = 4096 bytes (page boundary)
_stack_bottom:               # Label: beginning of stack space (highest addresses used first)
    .space 4096 * 4          # Reserve 16KB (16384 bytes) for stack
_stack_top:                  # Label: end of stack space (this is where SP starts)
