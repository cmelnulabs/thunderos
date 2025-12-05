Bootloader Implementation
=========================

ThunderOS uses a two-stage boot process with ``-bios none`` mode (no OpenSBI):

1. **M-mode entry** (``boot/entry.S``) - First code at 0x80000000
2. **S-mode boot** (``boot/boot.S``) - Kernel initialization

Boot Flow
---------

.. code-block:: text

   QEMU Reset (0x1000)
         │
         ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  M-mode Entry (entry.S)                                     │
   │  • Select hart 0, park others                               │
   │  • Setup M-mode stack                                       │
   │  • Call start() for hardware init                           │
   │  • mret → S-mode                                            │
   └─────────────────────────────────────────────────────────────┘
         │
         ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  S-mode Boot (boot.S)                                       │
   │  • Disable interrupts, setup stack                          │
   │  • Clear BSS section                                        │
   │  • Jump to kernel_main()                                    │
   └─────────────────────────────────────────────────────────────┘
         │
         ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  Kernel Init (kernel/main.c)                                │
   │  • UART, interrupts, timer                                  │
   │  • Memory management (PMM, paging, DMA)                     │
   │  • VirtIO devices, ext2 filesystem                          │
   │  • Launch shell (/bin/ush)                                  │
   └─────────────────────────────────────────────────────────────┘

M-mode Boot Stage
-----------------

With ``-bios none``, QEMU starts execution at ``0x80000000`` in Machine mode (M-mode),
the highest privilege level. ThunderOS must configure M-mode CSRs that OpenSBI would
normally set up.

Entry Point (entry.S)
~~~~~~~~~~~~~~~~~~~~~

**File:** ``boot/entry.S``

.. code-block:: asm

   .section .text.entry
   .global _entry
   _entry:
       # Read hartid - only hart 0 continues, others spin
       csrr a0, mhartid
       bnez a0, spin
       
       # Setup M-mode stack (16KB)
       la sp, stack0
       li a0, 4096 * 4
       add sp, sp, a0
       
       # Jump to C initialization
       call start
       
       j spin
   
   spin:
       wfi
       j spin

**Hart Selection:**
   QEMU may start multiple hardware threads (harts). Only hart 0 proceeds with boot;
   others enter an infinite ``wfi`` loop. This ensures single-threaded initialization.

**M-mode Stack:**
   A 16KB stack is reserved at ``stack0`` for M-mode C code (``start()``).

M-mode Initialization (start.c)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**File:** ``boot/start.c``

The ``start()`` function configures all M-mode Control and Status Registers (CSRs):

**1. Set Previous Privilege Mode:**

.. code-block:: c

   x = r_mstatus();
   x &= ~MSTATUS_MPP_MASK;  // Clear MPP field (bits 12:11)
   x |= MSTATUS_MPP_S;       // Set MPP = 01 (Supervisor)
   w_mstatus(x);

The ``mstatus.MPP`` field (bits 12:11) determines the privilege level after ``mret``.

* **Before:** MPP typically contains ``0b11`` (Machine mode) after reset
* **After:** MPP = ``0b01`` (Supervisor mode)
* All other ``mstatus`` bits remain unchanged

This ensures we'll be in Supervisor mode after the ``mret`` transition.

**2. Set Exception Program Counter:**

.. code-block:: c

   w_mepc((unsigned long)kernel_main);

``mepc`` holds the address where execution resumes after ``mret``. We set it to
``kernel_main()`` so execution jumps directly to the S-mode kernel entry point.

**3. Disable Paging:**

.. code-block:: c

   w_satp(0);

Clear the ``satp`` (Supervisor Address Translation and Protection) register to disable
paging initially. Virtual memory will be enabled later in ``kernel_main()`` after page
tables are set up.

**4. Delegate Exceptions to S-mode:**

.. code-block:: c

   w_medeleg(0xffff);  // Delegate all exceptions to S-mode

Setting all bits (0-15) in ``medeleg`` delegates all exception types to S-mode:

* **Bits 12, 13, 15** (page faults): Critical for virtual memory—the kernel must handle
  instruction, load, and store page faults for demand paging and memory protection
* **Bit 8** (ECALL from U-mode): Critical for syscalls—user programs call the kernel
  via ``ecall``, which must be handled in S-mode, not M-mode
* **Other bits**: Handle misaligned accesses, illegal instructions, breakpoints, etc.

Without delegation, M-mode would handle all these, but M-mode doesn't know about user
processes or virtual memory—only the kernel can manage them.

**5. Delegate Interrupts to S-mode:**

.. code-block:: c

   w_mideleg(0xffff);  // Delegate all interrupts to S-mode

Setting all bits (0-11) in ``mideleg`` delegates all interrupt types to S-mode:

* **Bit 5** (Timer): Critical for process scheduling—the kernel needs periodic timer
  interrupts to switch between processes
* **Bit 9** (External): Critical for device I/O—PLIC/UART/VirtIO interrupts must reach
  the kernel's interrupt handler
* **Bit 1** (Software): Needed for IPI (inter-processor interrupts) on multi-core systems

M-mode cannot manage timers or I/O in a general-purpose OS—these must be handled by
the kernel in S-mode.

**6. Enable S-mode Interrupts:**

.. code-block:: c

   w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

Enable interrupt handling in S-mode by setting three bits in the ``sie`` (Supervisor
Interrupt Enable) register:

* ``SIE_SEIE`` (bit 9): External interrupt enable—allows device interrupts (UART, VirtIO, etc.)
* ``SIE_STIE`` (bit 5): Timer interrupt enable—allows timer ticks for process scheduling
* ``SIE_SSIE`` (bit 1): Software interrupt enable—allows inter-processor interrupts (IPI)

**Why this matters:** These bits are prerequisites for interrupt delivery. However, they
don't actually enable interrupts yet—that happens later when we set ``sstatus.SIE`` in
S-mode. Think of ``sie`` as configuring *which* interrupts are eligible, and ``sstatus.SIE``
as the master on/off switch.

**7. Configure Physical Memory Protection (PMP):**

.. code-block:: c

   w_pmpaddr0(0x3fffffffffffffUL);  // All 56 bits of physical address space
   w_pmpcfg0(0xf);                   // R+W+X, TOR mode

**What is PMP?** Physical Memory Protection (PMP) is a hardware mechanism in RISC-V that
controls which physical memory regions lower privilege levels (S-mode and U-mode) can access.
By default, without PMP configuration, S-mode cannot access any physical memory—this would
make the kernel non-functional.

**Why we need it:** M-mode is configuring PMP to grant S-mode permission to access all
physical memory. Without this, the kernel would immediately fault when trying to read/write
memory or execute code.

**How it works:**

* ``pmpaddr0 = 0x3fffffffffffff``: This sets a physical address boundary at the maximum
  56-bit address (RISC-V supports up to 56-bit physical addresses). This is the "top of range."
  
* ``pmpcfg0 = 0xf``: This configures PMP entry 0:
  
  * **Bits 0-2 (value 0b111 = 7):** R=1 (read), W=1 (write), X=1 (execute) permissions
  * **Bits 3-4 (value 0b01 = 1):** A=TOR (Top-Of-Range addressing mode)
  * Combined: ``0xf = 0b1111`` = R+W+X with TOR mode

**TOR mode explained:** Top-Of-Range means "grant access from address 0 up to the address
in pmpaddr0." So this creates a single region covering all physical memory (0x0 to
0x3fffffffffffff) with full read, write, and execute permissions.

**Result:** S-mode can now freely access all physical memory, which allows the kernel to
manage RAM, load programs, handle I/O, etc.

**8. Initialize Timer:**

.. code-block:: c

   timerinit();

**What it does:** Configures the timer system so S-mode can handle timer interrupts for
process scheduling.

**Configuration steps performed:**

* ``MIE.STIE`` (bit 5): Enable S-mode timer interrupt in M-mode interrupt enable register.
  This allows timer interrupts to be delegated to S-mode.

* ``menvcfg.STCE`` (bit 63): Enable the SSTC (Supervisor Timer Compare) extension. This is
  a newer RISC-V feature that allows S-mode to write the ``stimecmp`` CSR directly.

* ``mcounteren.TM`` (bit 1): Allow S-mode to read the ``time`` CSR. The kernel needs to
  read the current time value to schedule processes and handle timeouts.

* ``stimecmp`` = ULONG_MAX: Initialize the timer compare register to maximum value.
  This prevents spurious timer interrupts before the kernel is ready to handle them.

* **Configure SIE (Supervisor Interrupt Enable):** Set bits for software and external
  interrupts, but explicitly clear the timer interrupt bit (bit 5). This prepares S-mode
  to handle device interrupts while preventing timer interrupts until ``hal_timer_init()``
  is ready to program the timer properly.

**Why SSTC matters:** Without SSTC, S-mode would need to trap to M-mode (via SBI calls)
every time it wants to program a timer. SSTC eliminates this overhead by letting S-mode
write ``stimecmp`` directly, which is critical for efficient process scheduling.

**9. Store Hart ID:**

.. code-block:: c

   int id = r_mhartid();
   w_tp(id);

**What is a hart?** Hart = Hardware Thread. A multi-core RISC-V processor may have multiple
harts, each capable of executing independently. Each hart has a unique ID (0, 1, 2, ...).

**Why store it?** The kernel needs to know which hart is executing its code. This is used for:

* Per-CPU data structures (each hart has its own kernel stack, scheduler state, etc.)
* Inter-processor interrupts (one hart sending work to another)
* Locking and synchronization (ensuring only one hart modifies shared data)

**Why use ``tp``?** The ``tp`` (thread pointer) register is preserved across privilege
level changes and is designated by convention as the per-hart identifier. S-mode code can
quickly read ``tp`` without a CSR instruction to determine which hart it's on.

**Current status:** ThunderOS currently only uses hart 0 (single-core), but this setup
enables future multi-core support.

**10. Transition to S-mode:**

.. code-block:: c

   asm volatile("mret");

**What is mret?** Machine Return (``mret``) is the RISC-V instruction that returns from
M-mode to a lower privilege level. It's typically used to return from M-mode trap handlers,
but here we use it as a one-way transition from M-mode to S-mode.

**What happens atomically:**

1. **Privilege level changes:** Current mode becomes the value in ``mstatus.MPP``
   (Supervisor mode, which we set in step 1)

2. **PC jumps:** Program counter becomes the value in ``mepc`` (``kernel_main``,
   which we set in step 2)

3. **Interrupt state restores:** ``mstatus.MIE`` (M-mode interrupt enable) is set to
   ``mstatus.MPIE`` (M-mode Previous Interrupt Enable). This is a legacy behavior of ``mret``
   that restores the previous interrupt enable state. In our case, MPIE was set at processor
   reset and we never modified it, so this just copies the reset value back to MIE. In
   practice, this doesn't matter here since we're not returning from a real trap—we're
   using ``mret`` as a one-way transition.

4. **Memory privilege cleared:** ``mstatus.MPRV`` is cleared, ensuring memory accesses
   use the current (S-mode) privilege level, not M-mode.

**Result:** Execution continues at the first instruction of ``kernel_main()`` in S-mode.
We can never return to M-mode (no M-mode handlers exist), and all M-mode configuration
is complete. The kernel now has full control.

M-mode CSR Summary
~~~~~~~~~~~~~~~~~~

The following table shows the order in which M-mode CSRs are configured in ``start()``,
the values written, and their purposes:

.. list-table::
   :header-rows: 1
   :widths: 15 20 25 40

   * - Step
     - CSR
     - Value
     - Purpose
   * - 1
     - ``mstatus.MPP``
     - 0b01 (S-mode)
     - Privilege level after mret (Supervisor mode)
   * - 2
     - ``mepc``
     - kernel_main
     - Program counter after mret (jump to kernel entry)
   * - 3
     - ``satp``
     - 0
     - Disable paging initially (enabled later in kernel)
   * - 4
     - ``medeleg``
     - 0xffff
     - Delegate all 16 exception types to S-mode
   * - 5
     - ``mideleg``
     - 0xffff
     - Delegate all interrupt types to S-mode
   * - 6
     - ``sie``
     - SEIE \| STIE \| SSIE
     - Enable S-mode external, timer, and software interrupts
   * - 7
     - ``pmpaddr0``
     - 0x3fffffffffffff
     - PMP region boundary (all 56-bit physical address space)
   * - 7
     - ``pmpcfg0``
     - 0xf
     - PMP permissions (R+W+X) and TOR addressing mode
   * - 8 (timerinit)
     - ``mie.STIE``
     - 1
     - Enable S-mode timer interrupts in M-mode
   * - 8 (timerinit)
     - ``menvcfg.STCE``
     - 1
     - Enable SSTC (Supervisor Timer Compare) extension
   * - 8 (timerinit)
     - ``mcounteren.TM``
     - 1
     - Allow S-mode to read time CSR
   * - 8 (timerinit)
     - ``stimecmp``
     - 0xFFFFFFFFFFFFFFFF
     - Set timer compare to maximum (prevent spurious interrupts)
   * - 8 (timerinit)
     - ``sie``
     - SEIE \| SSIE (STIE cleared)
     - Clear timer bit in SIE (timer enabled later by hal_timer_init)
   * - 9
     - ``tp``
     - mhartid
     - Store hart ID for S-mode identification
   * - 10
     - N/A (mret)
     - N/A
     - Transition to S-mode (atomically sets privilege, PC, interrupt state)

S-mode Bootloader (boot.S)
--------------------------

After ``mret``, execution continues at ``_start`` in ``boot.S``, now in S-mode.

The S-mode bootloader performs minimal initialization:

1. Disable interrupts
2. Setup stack pointer
3. Clear BSS section (uninitialized global variables)
4. Jump to C kernel (``kernel_main``)

Source Code
-----------

.. code-block:: asm

   /* boot/boot.S */
   .section .text.boot
   .global _start

   _start:
       # Disable interrupts
       csrw sie, zero
       
       # Setup stack pointer
       la sp, _stack_top
       
       # Clear BSS section
       la t0, _bss_start
       la t1, _bss_end
   clear_bss:
       beq t0, t1, clear_bss_done
       sd zero, 0(t0)
       addi t0, t0, 8
       j clear_bss
   clear_bss_done:
       
       # Jump to kernel main
       call kernel_main
       
       # If kernel_main returns, halt
   halt:
       wfi
       j halt

   .section .bss
   .align 12
   _stack_bottom:
       .space 4096 * 4  # 16KB stack
   _stack_top:

Detailed Analysis
-----------------

Step 1: Disable Interrupts
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   csrw sie, zero

**What it does:**
   Writes zero to the Supervisor Interrupt Enable (SIE) register

**Why:**
   * Kernel has no interrupt handlers configured yet
   * Taking an interrupt now would cause a trap to uninitialized handler
   * Interrupts will be re-enabled later when handlers are ready

**RISC-V Details:**
   For CSR instruction reference, see :doc:`../riscv/csr_registers` and :doc:`../riscv/instruction_set`.

Step 2: Setup Stack Pointer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   la sp, _stack_top

**What it does:**
   Loads the address of ``_stack_top`` into the stack pointer register (``sp``)

**Why:**
   * C functions need a valid stack for:
   
     * Function call return addresses
     * Local variables
     * Function arguments (beyond what fits in registers)
   
   * Without a stack, calling any function would crash

**RISC-V Details:**
   See :doc:`../riscv/assembly_guide` for stack conventions and calling convention details.

**Stack Layout:**

.. code-block:: text

   High Address
   ┌──────────────────┐ ← _stack_top (sp starts here)
   │                  │
   │   Stack grows    │
   │   downward ↓     │
   │                  │
   │   (16KB space)   │
   │                  │
   │                  │
   └──────────────────┘ ← _stack_bottom
   Low Address

Step 3: Clear BSS Section
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   la t0, _bss_start
   la t1, _bss_end
   clear_bss:
       beq t0, t1, clear_bss_done
       sd zero, 0(t0)
       addi t0, t0, 8
       j clear_bss
   clear_bss_done:

**What it does:**
   Zeros out all memory in the BSS section

**Why:**
   * **BSS** = Block Started by Symbol (historical name)
   * Contains uninitialized global/static variables
   * C standard requires these to be zero-initialized
   * Without clearing: variables have random garbage values

**Algorithm:**

.. code-block:: c

   // Equivalent C code:
   char *ptr = _bss_start;
   char *end = _bss_end;
   while (ptr != end) {
       *(uint64_t*)ptr = 0;
       ptr += 8;
   }

**Instruction Breakdown:**

.. code-block:: asm

   la t0, _bss_start      # t0 = start address
   la t1, _bss_end        # t1 = end address (loop boundary)
   
   clear_bss:
   beq t0, t1, clear_bss_done    # if (t0 == t1) goto done
   sd zero, 0(t0)                # store 8 bytes of zero at *t0
   addi t0, t0, 8                # t0 += 8 (advance pointer)
   j clear_bss                   # goto clear_bss (loop)
   
   clear_bss_done:

**Why 8 bytes at a time?**
   * RISC-V 64-bit: natural word size is 64 bits (8 bytes)
   * ``sd`` = Store Doubleword (64-bit store)
   * More efficient than clearing byte-by-byte

Step 4: Jump to Kernel
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   call kernel_main

**What it does:**
   Calls the C function ``kernel_main()`` defined in ``kernel/main.c``

**RISC-V Details:**
   * ``call`` is a pseudo-instruction that expands to:
   
     .. code-block:: asm
     
        auipc ra, offset[31:12]    # Load upper bits of target into ra
        jalr ra, ra, offset[11:0]  # Jump and link (save return address)
   
   * Return address saved in ``ra`` register (x1)
   * When ``kernel_main`` returns (if ever), execution continues at next instruction

Step 5: Halt Loop
~~~~~~~~~~~~~~~~~

.. code-block:: asm

   halt:
       wfi
       j halt

**What it does:**
   Infinite loop that puts CPU to sleep

**Why:**
   * ``kernel_main()`` should **never** return (it has its own infinite loop)
   * This is a safety net: if it does return, don't execute random memory
   * ``wfi`` = Wait For Interrupt (low-power state)
   * When interrupt arrives, CPU wakes briefly then loops back to ``wfi``

Stack Definition
~~~~~~~~~~~~~~~~

.. code-block:: asm

   .section .bss
   .align 12
   _stack_bottom:
       .space 4096 * 4  # 16KB stack
   _stack_top:

**Layout:**
   * Placed in ``.bss`` section (uninitialized data)
   * ``.align 12`` = align to 2^12 = 4096 bytes (page boundary)
   * ``.space 16384`` = reserve 16KB of memory
   * ``_stack_top`` immediately follows (highest address)

**Why 16KB?**
   * Enough for typical kernel stack usage
   * Deep call chains or large local arrays need more
   * Can be increased if needed (adjust ``.space`` directive)

Register Usage
--------------

The bootloader uses:

* ``sp`` (x2) - Stack pointer (set to ``_stack_top``)
* ``t0``/``t1`` (x5/x6) - Temporaries for BSS clearing loop
* ``ra`` (x1) - Return address (set by ``call``)

For complete register reference, see :doc:`../riscv/assembly_guide`.

Control Flow
------------

.. code-block:: text

   _start
     │
     ├─→ csrw sie, zero          (disable interrupts)
     │
     ├─→ la sp, _stack_top       (setup stack)
     │
     ├─→ clear_bss loop          (zero BSS section)
     │   │
     │   └─→ [loop until done]
     │
     ├─→ call kernel_main        (jump to C code)
     │
     └─→ halt loop               (should never reach)
         └─→ wfi (infinite)

Interaction with Linker Script
-------------------------------

The bootloader relies on symbols defined in ``kernel/arch/riscv64/kernel.ld``:

* ``_bss_start`` - Start address of BSS section
* ``_bss_end`` - End address of BSS section
* ``_stack_top`` - Defined in bootloader itself, placed by linker

The linker script ensures:

1. ``.text.entry`` is placed first (at 0x80000000), then ``.text.boot``
2. BSS section is properly aligned and sized
3. All symbols are resolved at link time

See :doc:`linker_script` for details.


See Also
--------

* :doc:`linker_script` - Memory layout and section placement
* :doc:`registers` - RISC-V register reference
* :doc:`../architecture` - Overall system architecture
