Assembly Programming Guide
===========================

Practical guide to writing RISC-V assembly code for OS development.

Calling Convention
------------------

Integer Registers
~~~~~~~~~~~~~~~~~

.. code-block:: text

   x0  (zero):  Always zero (hardwired)
   x1  (ra):    Return address
   x2  (sp):    Stack pointer
   x3  (gp):    Global pointer  
   x4  (tp):    Thread pointer
   x5-7 (t0-t2): Temporaries (caller-saved)
   x8  (s0/fp): Saved register / frame pointer (callee-saved)
   x9  (s1):    Saved register (callee-saved)
   x10-11 (a0-a1): Function args / return values (caller-saved)
   x12-17 (a2-a7): Function arguments (caller-saved)
   x18-27 (s2-s11): Saved registers (callee-saved)
   x28-31 (t3-t6): Temporaries (caller-saved)

Function Calls
~~~~~~~~~~~~~~

.. code-block:: asm

   # Caller
   li a0, 42        # First argument
   li a1, 100       # Second argument
   call function    # Call function
   # Result in a0
   
   # Callee
   function:
       addi sp, sp, -16
       sd ra, 0(sp)      # Save return address
       sd s0, 8(sp)      # Save s0
       
       # Function body...
       
       ld ra, 0(sp)      # Restore return address
       ld s0, 8(sp)      # Restore s0
       addi sp, sp, 16
       ret

Common Patterns
---------------

See :doc:`instruction_set` for detailed instruction examples.

Inline Assembly in C
--------------------

Basic Syntax
~~~~~~~~~~~~

.. code-block:: c

   asm volatile("instruction"
       : output operands
       : input operands
       : clobbers);

Examples
~~~~~~~~

.. code-block:: c

   // Read CSR
   unsigned long sstatus;
   asm volatile("csrr %0, sstatus" : "=r"(sstatus));
   
   // Write CSR
   unsigned long value = 0x2;
   asm volatile("csrw sstatus, %0" :: "r"(value));
   
   // Named register constraints
   register unsigned long a0 asm("a0") = value;
   asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");

See Also
--------

* :doc:`instruction_set` - Complete instruction reference
* :doc:`../internals/bootloader` - Real assembly code examples
* :doc:`../internals/trap_handler` - Trap handling assembly
