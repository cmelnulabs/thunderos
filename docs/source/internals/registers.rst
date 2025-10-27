RISC-V Registers in ThunderOS
==============================

This page documents how ThunderOS uses RISC-V registers.

.. note::
   For complete RISC-V register reference, see :doc:`../riscv/assembly_guide` and :doc:`../riscv/csr_registers`.

General Purpose Register Usage
-------------------------------

ThunderOS follows the standard RISC-V calling convention. See :doc:`../riscv/assembly_guide` for complete details.

Key Registers in ThunderOS Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Bootloader (boot.S):**

.. code-block:: asm

   la sp, _stack_top     # sp = stack pointer
   la t0, _bss_start     # t0 = loop pointer
   la t1, _bss_end       # t1 = loop bound
   sd zero, 0(t0)        # zero = constant 0

**C Function Calls:**

.. code-block:: c

   int add(int a, int b) {  // a0=a, a1=b
       return a + b;         // return in a0
   }
   
   int result = add(5, 7);   // 5→a0, 7→a1, result←a0

Control and Status Registers (CSRs)
------------------------------------

CSRs are special registers for system configuration and control.

Supervisor CSRs (S-mode)
~~~~~~~~~~~~~~~~~~~~~~~~~

These are accessible in supervisor mode (where ThunderOS runs):

.. list-table::
   :header-rows: 1
   :widths: 15 15 70

   * - Address
     - Name
     - Description
   * - 0x100
     - sstatus
     - Supervisor status (interrupt enable, privilege, etc.)
   * - 0x104
     - sie
     - Supervisor interrupt enable
   * - 0x105
     - stvec
     - Supervisor trap handler base address
   * - 0x140
     - sscratch
     - Scratch register for trap handler
   * - 0x141
     - sepc
     - Supervisor exception program counter
   * - 0x142
     - scause
     - Supervisor trap cause
   * - 0x143
     - stval
     - Supervisor trap value (bad address, etc.)
   * - 0x144
     - sip
     - Supervisor interrupt pending
   * - 0x180
     - satp
     - Supervisor address translation and protection

Control and Status Registers (CSRs)
------------------------------------

For complete CSR reference, see :doc:`../riscv/csr_registers`.

Key S-mode CSRs Used in ThunderOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Trap Handling:**

* ``stvec`` - Trap vector address (set in :doc:`trap_handler`)
* ``sepc`` - Exception program counter
* ``scause`` - Trap cause
* ``stval`` - Trap value
* ``sstatus`` - Status register (interrupt enable, privilege)

**Interrupt Configuration:**

* ``sie`` - Interrupt enable bits
* ``sip`` - Interrupt pending bits

See :doc:`trap_handler` and :doc:`timer_clint` for usage examples.

CSR Access Examples
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Read CSR
   unsigned long sstatus;
   asm volatile("csrr %0, sstatus" : "=r"(sstatus));
   
   // Write CSR
   unsigned long value = 0x2;
   asm volatile("csrw sstatus, %0" :: "r"(value));
   
   // Set bits
   asm volatile("csrsi sie, 0x20");  // Enable timer interrupt
   
   // Clear bits
   asm volatile("csrci sstatus, 0x2");  // Disable interrupts

Register Saving in ThunderOS
-----------------------------

Trap Frame
~~~~~~~~~~

The trap handler saves all 32 GPRs + CSRs. See :doc:`trap_handler` for complete details:

.. code-block:: c

   struct trap_frame {
       unsigned long ra;     // x1
       unsigned long sp;     // x2
       // ... all 32 registers ...
       unsigned long sepc;
       unsigned long sstatus;
   };

Stack Frame
~~~~~~~~~~~

C functions preserve callee-saved registers (s0-s11, sp):

.. code-block:: asm

   function:
       addi sp, sp, -16
       sd ra, 0(sp)      # Save return address
       sd s0, 8(sp)      # Save s0
       
       # Function body
       
       ld ra, 0(sp)      # Restore
       ld s0, 8(sp)
       addi sp, sp, 16
       ret

Register Debugging
------------------

Dumping Registers
~~~~~~~~~~~~~~~~~

.. code-block:: c

   void dump_trap_frame(struct trap_frame *frame) {
       uart_puts("Trap Frame:\n");
       uart_puts("  ra:  "); print_hex(frame->ra); uart_puts("\n");
       uart_puts("  sp:  "); print_hex(frame->sp); uart_puts("\n");
       uart_puts("  sepc: "); print_hex(frame->sepc); uart_puts("\n");
   }

GDB Commands
~~~~~~~~~~~~

.. code-block:: text

   info registers          # All general-purpose registers
   print/x $sp            # Print stack pointer
   print/x $ra            # Print return address
   
   # Examine CSRs
   info registers scause
   info registers sepc

See Also
--------

* :doc:`../riscv/assembly_guide` - Complete calling convention reference
* :doc:`../riscv/csr_registers` - All CSR details
* :doc:`trap_handler` - How registers are saved/restored
* :doc:`bootloader` - Register usage in assembly

