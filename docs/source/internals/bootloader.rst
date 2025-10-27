Bootloader Implementation
=========================

The bootloader is the first code executed by ThunderOS after the firmware (OpenSBI).
It is written in RISC-V assembly and located in ``boot/boot.S``.

Overview
--------

The bootloader performs minimal initialization:

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

Entry Point (_start)
~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   .section .text.boot
   .global _start

* Places code in ``.text.boot`` section (first section in linker script)
* Makes ``_start`` symbol globally visible (required by linker)
* OpenSBI jumps here after hardware initialization

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
   * Each function call uses ~16-64 bytes
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

1. ``.text.boot`` is placed first (at 0x80200000)
2. BSS section is properly aligned and sized
3. All symbols are resolved at link time

See :doc:`linker_script` for details.

Common Issues
-------------

Stack Overflow
~~~~~~~~~~~~~~

**Symptom:** Random crashes, corrupted data

**Cause:** Stack grows into other memory regions

**Solution:** Increase stack size in ``boot.S``

.. code-block:: asm

   .space 4096 * 16  # 64KB stack instead of 16KB

Unaligned Access
~~~~~~~~~~~~~~~~

**Symptom:** Trap/exception on BSS clear loop

**Cause:** ``_bss_start`` not aligned to 8 bytes

**Solution:** Ensure linker script aligns BSS section:

.. code-block:: ld

   .bss : ALIGN(8) {
       _bss_start = .;
       *(.bss)
       _bss_end = .;
   }

Missing Symbols
~~~~~~~~~~~~~~~

**Symptom:** Linker error "undefined reference to _bss_start"

**Cause:** Linker script doesn't define required symbols

**Solution:** Add ``PROVIDE(_bss_start = .);`` in linker script

Performance Notes
-----------------

BSS Clear Optimization
~~~~~~~~~~~~~~~~~~~~~~

Current implementation clears 8 bytes per iteration. Could be optimized:

**Option 1: Use vector instructions (RVV)**

.. code-block:: asm

   # Requires RISC-V Vector Extension
   vsetvli t2, t1, e64, m1
   vmv.v.i v0, 0
   loop:
     vse64.v v0, (t0)
     add t0, t0, t2
     blt t0, t1, loop

**Option 2: Unroll loop**

.. code-block:: asm

   clear_bss:
       beq t0, t1, clear_bss_done
       sd zero, 0(t0)
       sd zero, 8(t0)
       sd zero, 16(t0)
       sd zero, 24(t0)
       addi t0, t0, 32
       j clear_bss

**Trade-off:** Code size vs. speed (minimal benefit for small BSS)

Testing
-------

To verify bootloader works:

1. **Build and run:**

   .. code-block:: bash
   
      make clean && make qemu

2. **Expected output:**

   .. code-block:: text
   
      OpenSBI v0.9
      ...
      ThunderOS - RISC-V AI OS
      Kernel loaded at 0x80200000
      [OK] UART initialized

3. **Debug with GDB:**

   .. code-block:: bash
   
      make debug
      # In another terminal:
      riscv64-unknown-elf-gdb build/thunderos.elf
      (gdb) target remote :1234
      (gdb) break _start
      (gdb) continue
      (gdb) stepi   # Step through bootloader
      (gdb) info registers sp

Future Enhancements
-------------------

Potential improvements:

* **Multi-core boot:** Wake up additional CPU cores
* **Device tree parsing:** Read hardware configuration
* **Memory detection:** Query available RAM from firmware
* **Secure boot:** Verify kernel signature before jumping
* **Early console:** Print bootloader progress messages

See Also
--------

* :doc:`linker_script` - Memory layout and section placement
* :doc:`registers` - RISC-V register reference
* :doc:`../architecture` - Overall system architecture
