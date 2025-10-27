Trap Handler
============

The trap handler is the kernel's mechanism for responding to exceptions and interrupts in RISC-V. It provides the infrastructure to handle both synchronous exceptions (like page faults, illegal instructions) and asynchronous interrupts (like timer interrupts, external device interrupts).

Overview
--------

In RISC-V, the term "trap" encompasses both:

* **Exceptions**: Synchronous events caused by instruction execution (e.g., page fault, illegal instruction, ecall)
* **Interrupts**: Asynchronous events from external sources (e.g., timer, UART, external devices)

When a trap occurs, the hardware:

1. Sets ``sepc`` (Supervisor Exception Program Counter) to the return address
2. Sets ``scause`` to indicate the trap cause
3. Sets ``stval`` with additional trap-specific information
4. Disables interrupts by clearing ``sstatus.SIE``
5. Jumps to the address in ``stvec`` (Supervisor Trap Vector)

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

The trap frame is a data structure that stores the complete CPU state when a trap occurs. This allows the handler to examine or modify the interrupted context.

.. code-block:: c

   // include/trap.h
   struct trap_frame {
       unsigned long ra;    // x1:  Return address
       unsigned long sp;    // x2:  Stack pointer
       unsigned long gp;    // x3:  Global pointer
       unsigned long tp;    // x4:  Thread pointer
       unsigned long t0;    // x5:  Temporary 0
       unsigned long t1;    // x6:  Temporary 1
       unsigned long t2;    // x7:  Temporary 2
       unsigned long s0;    // x8:  Saved 0 / Frame pointer
       unsigned long s1;    // x9:  Saved 1
       unsigned long a0;    // x10: Argument 0 / Return value 0
       unsigned long a1;    // x11: Argument 1 / Return value 1
       unsigned long a2;    // x12: Argument 2
       unsigned long a3;    // x13: Argument 3
       unsigned long a4;    // x14: Argument 4
       unsigned long a5;    // x15: Argument 5
       unsigned long a6;    // x16: Argument 6
       unsigned long a7;    // x17: Argument 7
       unsigned long s2;    // x18: Saved 2
       unsigned long s3;    // x19: Saved 3
       unsigned long s4;    // x20: Saved 4
       unsigned long s5;    // x21: Saved 5
       unsigned long s6;    // x22: Saved 6
       unsigned long s7;    // x23: Saved 7
       unsigned long s8;    // x24: Saved 8
       unsigned long s9;    // x25: Saved 9
       unsigned long s10;   // x26: Saved 10
       unsigned long s11;   // x27: Saved 11
       unsigned long t3;    // x28: Temporary 3
       unsigned long t4;    // x29: Temporary 4
       unsigned long t5;    // x30: Temporary 5
       unsigned long t6;    // x31: Temporary 6
       unsigned long sepc;  // Supervisor Exception Program Counter
       unsigned long sstatus; // Supervisor Status Register
   };

**Size**: 34 registers × 8 bytes = 272 bytes per trap frame

Assembly Trap Vector
---------------------

The trap vector (``trap_vector``) is the assembly code that the hardware jumps to when a trap occurs. It's responsible for saving and restoring context.

Location
~~~~~~~~

Defined in ``kernel/arch/riscv64/trap_entry.S``.

Code Structure
~~~~~~~~~~~~~~

.. code-block:: asm

   .section .text
   .globl trap_vector
   .align 4
   trap_vector:
       # Allocate space for trap frame on stack
       addi sp, sp, -272
       
       # Save all general-purpose registers (x1-x31)
       sd ra,   0(sp)    # x1
       sd sp,   8(sp)    # x2 (note: sp before adjustment)
       sd gp,  16(sp)    # x3
       sd tp,  24(sp)    # x4
       sd t0,  32(sp)    # x5
       ...
       sd t6, 240(sp)    # x31
       
       # Save exception PC and status
       csrr t0, sepc
       sd t0, 248(sp)
       
       csrr t0, sstatus
       sd t0, 256(sp)
       
       # Call C trap handler with trap frame pointer
       mv a0, sp
       call trap_handler
       
       # Restore sstatus and sepc
       ld t0, 256(sp)
       csrw sstatus, t0
       
       ld t0, 248(sp)
       csrw sepc, t0
       
       # Restore all general-purpose registers
       ld ra,   0(sp)
       ld sp,   8(sp)
       ...
       ld t6, 240(sp)
       
       # Free trap frame space
       addi sp, sp, 272
       
       # Return from trap
       sret

Key Points
~~~~~~~~~~

* **Alignment**: The trap vector must be 4-byte aligned (``wfi`` mode) or page-aligned (``vectored`` mode)
* **Stack Usage**: Uses the current stack pointer (assumes stack is already set up)
* **Register Preservation**: Saves ALL 32 general-purpose registers to ensure complete context preservation
* **CSR Access**: Uses ``csrr`` (CSR read) and ``csrw`` (CSR write) to access special registers
* **Frame Pointer**: The trap frame pointer is passed to the C handler in register ``a0``

C Trap Handler
--------------

The C trap handler (``trap_handler``) examines the trap cause and dispatches to appropriate handlers.

Implementation
~~~~~~~~~~~~~~

.. code-block:: c

   // kernel/arch/riscv64/trap.c
   void trap_handler(struct trap_frame *frame) {
       unsigned long scause;
       
       // Read trap cause
       asm volatile("csrr %0, scause" : "=r"(scause));
       
       // Check if interrupt or exception
       if (scause & (1UL << 63)) {
           // Interrupt (MSB set)
           handle_interrupt(scause & 0x7FFFFFFFFFFFFFFF, frame);
       } else {
           // Exception (MSB clear)
           handle_exception(scause, frame);
       }
   }

Exception Handler
~~~~~~~~~~~~~~~~~

.. code-block:: c

   static void handle_exception(unsigned long cause, struct trap_frame *frame) {
       uart_puts("Exception occurred!\n");
       uart_puts("Cause: ");
       
       switch (cause) {
       case CAUSE_MISALIGNED_FETCH:
           uart_puts("Instruction address misaligned");
           break;
       case CAUSE_FETCH_ACCESS:
           uart_puts("Instruction access fault");
           break;
       case CAUSE_ILLEGAL_INSTRUCTION:
           uart_puts("Illegal instruction");
           break;
       case CAUSE_BREAKPOINT:
           uart_puts("Breakpoint");
           break;
       case CAUSE_MISALIGNED_LOAD:
           uart_puts("Load address misaligned");
           break;
       case CAUSE_LOAD_ACCESS:
           uart_puts("Load access fault");
           break;
       case CAUSE_MISALIGNED_STORE:
           uart_puts("Store address misaligned");
           break;
       case CAUSE_STORE_ACCESS:
           uart_puts("Store access fault");
           break;
       case CAUSE_USER_ECALL:
           uart_puts("Environment call from U-mode");
           break;
       case CAUSE_SUPERVISOR_ECALL:
           uart_puts("Environment call from S-mode");
           break;
       case CAUSE_MACHINE_ECALL:
           uart_puts("Environment call from M-mode");
           break;
       case CAUSE_FETCH_PAGE_FAULT:
           uart_puts("Instruction page fault");
           break;
       case CAUSE_LOAD_PAGE_FAULT:
           uart_puts("Load page fault");
           break;
       case CAUSE_STORE_PAGE_FAULT:
           uart_puts("Store page fault");
           break;
       default:
           uart_puts("Unknown");
           break;
       }
       
       uart_puts("\n");
       
       // Currently halt on exceptions (no recovery yet)
       while (1) {
           asm volatile("wfi");
       }
   }

Interrupt Handler
~~~~~~~~~~~~~~~~~

.. code-block:: c

   static void handle_interrupt(unsigned long cause, struct trap_frame *frame) {
       switch (cause) {
       case IRQ_S_TIMER:
           // Timer interrupt from CLINT
           clint_handle_timer();
           break;
       case IRQ_S_SOFT:
           // Software interrupt (IPI)
           uart_puts("Software interrupt\n");
           break;
       case IRQ_S_EXT:
           // External interrupt from PLIC
           uart_puts("External interrupt\n");
           break;
       default:
           uart_puts("Unknown interrupt: ");
           // Print cause value
           uart_puts("\n");
           break;
       }
   }

Trap Causes
-----------

RISC-V defines standard trap cause codes in the ``scause`` CSR.

Exception Causes
~~~~~~~~~~~~~~~~

.. code-block:: c

   #define CAUSE_MISALIGNED_FETCH       0
   #define CAUSE_FETCH_ACCESS           1
   #define CAUSE_ILLEGAL_INSTRUCTION    2
   #define CAUSE_BREAKPOINT             3
   #define CAUSE_MISALIGNED_LOAD        4
   #define CAUSE_LOAD_ACCESS            5
   #define CAUSE_MISALIGNED_STORE       6
   #define CAUSE_STORE_ACCESS           7
   #define CAUSE_USER_ECALL             8
   #define CAUSE_SUPERVISOR_ECALL       9
   #define CAUSE_MACHINE_ECALL         11
   #define CAUSE_FETCH_PAGE_FAULT      12
   #define CAUSE_LOAD_PAGE_FAULT       13
   #define CAUSE_STORE_PAGE_FAULT      15

Interrupt Causes
~~~~~~~~~~~~~~~~

.. code-block:: c

   #define IRQ_S_SOFT    1  // Supervisor software interrupt
   #define IRQ_S_TIMER   5  // Supervisor timer interrupt
   #define IRQ_S_EXT     9  // Supervisor external interrupt

The interrupt bit (bit 63) distinguishes interrupts from exceptions.

Initialization
--------------

The trap handler must be initialized during kernel boot.

Setup Process
~~~~~~~~~~~~~

.. code-block:: c

   // kernel/arch/riscv64/trap.c
   void trap_init(void) {
       extern void trap_vector(void);
       
       // Set trap vector address
       asm volatile("csrw stvec, %0" :: "r"(trap_vector));
       
       uart_puts("Trap handler initialized\n");
   }

This sets the ``stvec`` (Supervisor Trap Vector) CSR to point to our trap handler.

Called from ``kernel_main()``:

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       trap_init();    // Initialize trap handling
       // ... rest of initialization
   }

CSR Registers
-------------

Supervisor Trap Registers
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - CSR
     - Name
     - Description
   * - ``stvec``
     - Trap Vector
     - Address of trap handler (aligned to 4 bytes or page)
   * - ``sepc``
     - Exception PC
     - Address to return to after trap (``sret`` jumps here)
   * - ``scause``
     - Trap Cause
     - Reason for trap (exception code or interrupt number)
   * - ``stval``
     - Trap Value
     - Additional information (e.g., faulting address for page fault)
   * - ``sstatus``
     - Status Register
     - Privilege level, interrupt enable, etc.
   * - ``sie``
     - Interrupt Enable
     - Which interrupts are enabled
   * - ``sip``
     - Interrupt Pending
     - Which interrupts are pending

stvec Modes
~~~~~~~~~~~

The ``stvec`` register has two modes (controlled by lowest 2 bits):

* **Direct (0)**: All traps jump to ``BASE`` address
* **Vectored (1)**: Exceptions to ``BASE``, interrupts to ``BASE + 4*cause``

ThunderOS currently uses **Direct mode** (simpler, single entry point).

Interrupt Enable
----------------

Enabling Interrupts
~~~~~~~~~~~~~~~~~~~

To receive interrupts, two levels must be enabled:

1. **Global interrupt enable** in ``sstatus.SIE`` bit
2. **Specific interrupt enable** in ``sie`` register

.. code-block:: c

   // Enable timer interrupts
   void enable_timer_interrupts(void) {
       unsigned long sie;
       
       // Enable supervisor timer interrupts in sie
       asm volatile("csrr %0, sie" : "=r"(sie));
       sie |= (1 << 5);  // STIE bit
       asm volatile("csrw sie, %0" :: "r"(sie));
       
       // Enable global interrupts in sstatus
       unsigned long sstatus;
       asm volatile("csrr %0, sstatus" : "=r"(sstatus));
       sstatus |= (1 << 1);  // SIE bit
       asm volatile("csrw sstatus, %0" :: "r"(sstatus));
   }

The CLINT timer driver handles this automatically during initialization.

Disabling Interrupts
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Disable all interrupts
   asm volatile("csrci sstatus, 0x2");  // Clear SIE bit
   
   // Enable all interrupts
   asm volatile("csrsi sstatus, 0x2");  // Set SIE bit

Critical Sections
~~~~~~~~~~~~~~~~~

Code that must be atomic should disable interrupts:

.. code-block:: c

   void critical_operation(void) {
       unsigned long flags;
       
       // Save current interrupt state and disable
       asm volatile("csrr %0, sstatus" : "=r"(flags));
       asm volatile("csrci sstatus, 0x2");
       
       // ... critical section ...
       
       // Restore interrupt state
       if (flags & 0x2) {
           asm volatile("csrsi sstatus, 0x2");
       }
   }

Nested Traps
------------

Current Behavior
~~~~~~~~~~~~~~~~

ThunderOS currently does **not** support nested traps. When a trap occurs:

* ``sstatus.SIE`` is automatically cleared (disables interrupts)
* Traps execute with interrupts disabled
* ``sret`` automatically restores ``sstatus`` (re-enables interrupts)

This prevents trap handlers from being interrupted.

Future Enhancement
~~~~~~~~~~~~~~~~~~

To support nested traps:

1. Re-enable interrupts in trap handler (set ``sstatus.SIE``)
2. Use separate interrupt stack to avoid overflow
3. Track nesting depth to prevent infinite recursion

This is needed for:

* High-priority interrupts preempting low-priority handlers
* Handling page faults in kernel code
* Better interrupt latency

Testing
-------

The trap handler has a comprehensive test suite in ``tests/test_trap.c``.

Test Coverage
~~~~~~~~~~~~~

.. code-block:: c

   // Test basic trap initialization
   KUNIT_CASE(test_trap_initialized)
   
   // Test that trap handler is installed
   KUNIT_CASE(test_trap_handler_installed)
   
   // Test that normal code execution works
   KUNIT_CASE(test_basic_arithmetic)
   
   // Test that memory access works
   KUNIT_CASE(test_pointer_operations)

Running Tests
~~~~~~~~~~~~~

.. code-block:: bash

   cd tests
   make
   make run-test-trap

Expected output:

.. code-block:: text

   [ RUN      ] test_trap_initialized
   [       OK ] test_trap_initialized
   [ RUN      ] test_trap_handler_installed
   [       OK ] test_trap_handler_installed
   [ RUN      ] test_basic_arithmetic
   [       OK ] test_basic_arithmetic
   [ RUN      ] test_pointer_operations
   [       OK ] test_pointer_operations
   
   Total:  4
   Passed: 4
   Failed: 0

Debugging Traps
---------------

Print Trap Frame
~~~~~~~~~~~~~~~~

.. code-block:: c

   void print_trap_frame(struct trap_frame *frame) {
       uart_puts("Trap Frame:\n");
       uart_puts("  ra:  "); print_hex(frame->ra); uart_puts("\n");
       uart_puts("  sp:  "); print_hex(frame->sp); uart_puts("\n");
       // ... print other registers ...
       uart_puts("  sepc: "); print_hex(frame->sepc); uart_puts("\n");
   }

Examine in GDB
~~~~~~~~~~~~~~

.. code-block:: gdb

   # Break on trap handler
   (gdb) break trap_handler
   
   # Examine trap frame
   (gdb) print *(struct trap_frame *)$a0
   
   # Check cause
   (gdb) info registers scause
   (gdb) info registers sepc
   (gdb) info registers stval

Force Test Trap
~~~~~~~~~~~~~~~

.. code-block:: c

   // Trigger illegal instruction exception
   asm volatile(".word 0x00000000");
   
   // Trigger breakpoint exception
   asm volatile("ebreak");
   
   // Trigger ecall exception
   asm volatile("ecall");

Future Enhancements
-------------------

Planned improvements to the trap handling system:

1. **Exception Recovery**
   
   * Handle recoverable exceptions (e.g., page faults)
   * Resume execution after fixing the issue
   * Proper error reporting instead of halting

2. **Interrupt Priority**
   
   * Support nested interrupts
   * Priority levels for different interrupt sources
   * Preemptive interrupt handling

3. **Performance Counters**
   
   * Track trap frequency
   * Measure trap handler overhead
   * Identify hot paths

4. **Vector Support**
   
   * Save/restore vector registers (when RVV is used)
   * Context switch vector state
   * Lazy vector save (only if used)

5. **User Mode Support**
   
   * Handle traps from user mode
   * System call interface (``ecall``)
   * User/kernel boundary protection

References
----------

* **RISC-V Privileged Specification**: Detailed trap handling mechanisms
* **SBI Specification**: Supervisor Binary Interface for M-mode services  
* **OpenSBI Documentation**: Implementation details of the SBI runtime

See Also
--------

* :doc:`timer_clint` - Timer interrupts using the trap handler
* :doc:`testing_framework` - How trap handler tests work
* :doc:`registers` - RISC-V CSR register details
