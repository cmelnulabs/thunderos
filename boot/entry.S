/*
 * M-mode Entry Point for ThunderOS
 * =================================
 * 
 * File: boot/entry.S
 * Section: .text.entry (placed first in binary by linker script)
 * 
 * This is the VERY FIRST code that executes when QEMU starts the kernel.
 * With -bios none, QEMU loads our ELF directly into RAM at 0x80000000
 * and begins execution here in Machine mode (M-mode), the highest
 * privilege level in RISC-V.
 * 
 * Why M-mode?
 * -----------
 * RISC-V has three privilege levels:
 *   - M-mode (Machine):    Highest privilege, full hardware access
 *   - S-mode (Supervisor): OS kernel runs here
 *   - U-mode (User):       Applications run here
 * 
 * On reset, RISC-V CPUs always start in M-mode. Normally, firmware like
 * OpenSBI handles M-mode setup. With -bios none, WE must do this setup
 * before transitioning to S-mode where the kernel runs.
 * 
 * What this file does:
 * --------------------
 * 1. Select hart 0 as the boot processor (park other harts)
 * 2. Set up a stack for M-mode C code
 * 3. Call start() in start.c to configure M-mode CSRs
 * 4. start() will mret to S-mode, jumping to kernel_main()
 * 
 * Code size: ~32 bytes of instructions + 16KB stack reservation
 * 
 * Boot chain:
 *   QEMU reset → _entry (this file) → start() → mret → kernel_main()
 */

    .section .text.entry    /* Linker places this section first at 0x80000000 */
    .global _entry          /* Export symbol so linker can set it as entry point */

/*
 * _entry - Machine mode entry point
 * 
 * Executed at: 0x80000000 (defined by linker script)
 * Privilege:   M-mode (Machine mode)
 * Registers:   Undefined state on entry (except PC)
 * 
 * QEMU with -bios none jumps directly here after loading the ELF.
 * All harts (hardware threads) start executing here simultaneously.
 */
_entry:
    /* ======================================================================
     * Step 1: Hart Selection (Multi-core handling)
     * ======================================================================
     * 
     * Problem: QEMU may start multiple harts (CPU cores), all executing
     *          this same code simultaneously. We only want ONE hart to
     *          do the boot initialization.
     * 
     * Solution: Only hart 0 continues; all others enter spin loop.
     * 
     * mhartid CSR: Read-only register containing the hardware thread ID.
     *              Hart 0 = boot processor, others = secondary processors.
     */
    csrr a0, mhartid        /* a0 = mhartid (hart ID: 0, 1, 2, ...) */
    bnez a0, spin           /* if (a0 != 0) goto spin; // Park non-boot harts */

    /* ======================================================================
     * Step 2: Setup M-mode Stack
     * ======================================================================
     * 
     * We need a valid stack before calling any C functions (start()).
     * 
     * Stack layout in memory:
     * 
     *   stack0 + 16KB  ────────────────  ← sp points here (stack top)
     *                  │              │
     *                  │  Stack grows │
     *                  │  downward ↓  │
     *                  │              │
     *   stack0         ────────────────  ← stack bottom (defined below)
     * 
     * Why 16KB?
     *   - start() and its callees need stack space for:
     *     - Return addresses (ra register saved on stack)
     *     - Local variables
     *     - Function arguments beyond a0-a7
     *   - 16KB is generous for our simple M-mode initialization
     * 
     * Note: This is a SEPARATE stack from the S-mode kernel stack!
     *       S-mode will use _stack_top defined in boot.S.
     */
    la sp, stack0           /* sp = address of stack0 (stack bottom) */
    li a0, 4096 * 4         /* a0 = 16384 (16KB = 4 pages × 4KB) */
    add sp, sp, a0          /* sp = stack0 + 16KB (now points to stack top) */

    /* ======================================================================
     * Step 3: Jump to C initialization
     * ======================================================================
     * 
     * Now we have a valid stack, we can call C code!
     * 
     * start() in boot/start.c will:
     *   1. Configure mstatus.MPP = S-mode (for mret destination)
     *   2. Set mepc = kernel_main (return address for mret)
     *   3. Delegate exceptions/interrupts to S-mode
     *   4. Configure PMP (Physical Memory Protection)
     *   5. Initialize timer (SSTC extension)
     *   6. Execute mret (returns to S-mode at kernel_main)
     * 
     * start() should NEVER return here - it does mret to S-mode.
     */
    call start              /* Call start() in start.c */

    /* ======================================================================
     * Step 4: Fallback (should never reach here)
     * ======================================================================
     * 
     * If start() somehow returns (bug!), fall into spin loop.
     * This prevents executing garbage memory.
     */
    j spin                  /* Jump to spin (safety net) */

/*
 * spin - Infinite wait loop for parked harts
 * 
 * Purpose:
 *   - Park non-boot harts (hart 1, 2, 3, ...)
 *   - Safety net if start() unexpectedly returns
 * 
 * wfi (Wait For Interrupt):
 *   - Puts the hart into low-power sleep state
 *   - Wakes briefly on any interrupt, then loops back
 *   - More power-efficient than busy-waiting (j spin alone)
 * 
 * Future SMP support:
 *   To enable multi-core, the kernel would:
 *   1. Set up per-hart stacks and data structures
 *   2. Write hart IDs to a "go" variable
 *   3. Send inter-processor interrupts (IPI) to wake parked harts
 *   4. Parked harts check "go" variable and jump to kernel
 */
spin:
    wfi                     /* Wait For Interrupt (low-power sleep) */
    j spin                  /* Loop forever */

    /* ======================================================================
     * M-mode Stack Reservation
     * ======================================================================
     * 
     * .align 4 = align to 2^4 = 16 bytes (RISC-V ABI requirement)
     * 
     * The stack pointer must be 16-byte aligned when calling functions.
     * This ensures proper alignment for:
     *   - 64-bit load/store instructions (ld/sd)
     *   - Potential 128-bit values (future extensions)
     * 
     * .space 16384 = reserve 16KB of uninitialized memory
     * 
     * This goes in .text.entry section, which means it's placed right
     * after the code above in memory. The linker script handles this.
     */
    .align 4                /* 16-byte alignment for stack */
    .global stack0          /* Export so start.c can reference if needed */
stack0:
    .space 4096 * 4         /* Reserve 16KB (4 pages × 4KB per page) */

/*
 * Memory layout after linking:
 * ============================
 * 
 * 0x80000000  _entry          (this file - 32 bytes of code)
 * 0x80000020  stack0          (16KB M-mode stack)
 * 0x80004020  _start          (boot.S - S-mode bootloader)
 *             ...
 * 0x8001XXXX  kernel_main     (kernel/main.c)
 *             ...
 * 0x80025000  _bss_start      (uninitialized data begins)
 *             ...
 * 0x80054020  _bss_end        (uninitialized data ends)
 * 0x88000000                  (end of RAM - 128MB total)
 */
