Trap Handler
============

The trap handler responds to exceptions and interrupts in ThunderOS. It saves CPU state, identifies the trap cause, dispatches to appropriate handlers, and restores state.

For RISC-V trap fundamentals, see :doc:`../riscv/interrupts_exceptions`.

Overview
--------

When a trap occurs in ThunderOS:

1. Hardware saves ``sepc``, sets ``scause``/``stval``, jumps to ``stvec``
2. Assembly saves all registers to trap frame
3. C handler examines cause and dispatches
4. Assembly restores registers and returns with ``sret``

For RISC-V trap mechanism details, see :doc:`../riscv/interrupts_exceptions`.

Architecture
------------

Components
~~~~~~~~~~

The trap handling system consists of three main components:

.. code-block:: text

   kernel/arch/riscv64/trap_entry.S  - Assembly trap vector and context save/restore
   kernel/arch/riscv64/trap.c        - C trap handler logic
   include/trap.h                    - Trap frame structure and constants

Trap Flow
~~~~~~~~~

.. code-block:: text

   Hardware Trap Occurs
         |
         v
   [trap_vector] (trap_entry.S)
         |
         ├─> Save all 32 registers to trap_frame
         ├─> Save sepc (return address)
         ├─> Save sstatus (status register)
         |
         v
   [trap_handler()] (trap.c)
         |
         ├─> Read scause register
         ├─> Check if exception or interrupt
         |
         ├──> Exception: [handle_exception()]
         │         ├─> Print exception info
         │         └─> Halt system (currently)
         |
         └──> Interrupt: [handle_interrupt()]
               ├─> Timer interrupt → clint_handle_timer()
               ├─> Software interrupt → (TODO)
               └─> External interrupt → (TODO)
         |
         v
   [trap_vector restore] (trap_entry.S)
         |
         ├─> Restore sstatus
         ├─> Restore sepc
         ├─> Restore all 32 registers
         |
         v
   sret (return to interrupted code)

Trap Frame Structure
--------------------

Location: ``include/trap.h``

The trap frame stores all CPU registers (x1-x31), ``sepc``, and ``sstatus`` when a trap occurs.

**Size**: 272 bytes (34 registers × 8 bytes)

Assembly Trap Vector
---------------------

Location: ``kernel/arch/riscv64/trap_entry.S``

Saves all registers to stack, calls ``trap_handler()``, then restores registers and returns with ``sret``.

For RISC-V trap instructions, see :doc:`../riscv/instruction_set`.

C Trap Handler
--------------

Location: ``kernel/arch/riscv64/trap.c``

Reads ``scause`` to determine if trap is an exception or interrupt, then dispatches to ``handle_exception()`` or ``handle_interrupt()``.

Trap Causes
-----------

ThunderOS currently handles:

**Exceptions:**

* Illegal instruction (cause 2)
* Supervisor ecall (cause 9) 
* Page faults (cause 12, 13, 15)

**Interrupts:**

* Timer interrupt (IRQ 5) - see :doc:`timer_clint`
* External interrupt (IRQ 9) - future

For complete cause code reference, see :doc:`../riscv/interrupts_exceptions`.

Initialization
--------------

Called from ``kernel_main()`` to set ``stvec`` to point to ``trap_vector``.

See :doc:`../riscv/csr_registers` for CSR details.

CSR Usage
---------

ThunderOS uses these supervisor CSRs for trap handling:

* **stvec** - Points to ``trap_vector``
* **scause** - Trap cause identifier
* **sepc** - Return address
* **stval** - Fault addresses
* **sstatus** - Processor status
* **sie** - Interrupt enable

For CSR details, see :doc:`../riscv/csr_registers`.

Interrupt Control
~~~~~~~~~~~~~~~~~

Timer interrupts are enabled in :doc:`timer_clint`.

For interrupt mechanism details, see :doc:`../riscv/interrupts_exceptions`.

Nested Traps
------------

ThunderOS currently disables interrupts during trap handling (``sstatus.SIE`` cleared by hardware). Future enhancement will support nested traps for preemptive interrupt handling.

Testing
-------

See :doc:`testing_framework` for trap handler test suite and instructions.

Debugging Traps
---------------

See :doc:`testing_framework` for debugging and GDB usage.

Future Enhancements
-------------------

* Exception recovery (handle page faults gracefully)
* Nested interrupt support with priority levels
* User mode trap handling and system calls
* Performance counters for trap frequency tracking

See Also
--------

* :doc:`timer_clint` - Timer interrupts using trap handler
* :doc:`../riscv/interrupts_exceptions` - RISC-V trap mechanism
* :doc:`../riscv/csr_registers` - CSR reference
* :doc:`testing_framework` - Trap handler tests
