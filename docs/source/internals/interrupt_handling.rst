Interrupt Handling System
=========================

Overview
--------

ThunderOS implements a comprehensive interrupt handling system for RISC-V using:

* **PLIC** (Platform-Level Interrupt Controller) - for external device interrupts
* **CLINT** (Core-Local Interruptor) - for timer and software interrupts
* **Trap Handler** - unified exception and interrupt dispatcher

Architecture
------------

The interrupt system has three layers:

1. **Hardware Layer** (PLIC/CLINT drivers)
   - Direct interaction with memory-mapped interrupt controller registers
   - Low-level configuration and control

2. **Interrupt Management Layer** (interrupt.c)
   - Handler registration and dispatch
   - Priority management
   - Enable/disable individual IRQs

3. **Trap Handler Layer** (trap.c)
   - Unified entry point for all traps (exceptions and interrupts)
   - Context save/restore
   - Dispatch to appropriate handler

Components
----------

PLIC (Platform-Level Interrupt Controller)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**File:** ``kernel/arch/riscv64/drivers/plic.c``

**Responsibilities:**

* Manage external device interrupts (IRQ 1-127)
* Configure interrupt priorities (0-7)
* Enable/disable specific interrupts
* Claim and complete interrupt processing

**PLIC Constants:**

.. code-block:: c

    #define PLIC_MAX_IRQ  128  /* Maximum interrupt sources (0-127) */

The PLIC supports up to 128 interrupt sources (IRQ 0-127):

- **IRQ 0**: Reserved (no interrupt)
- **IRQ 1-127**: Available for devices
- Common IRQs on QEMU virt machine:
  
  * IRQ 1: VirtIO block device #0
  * IRQ 2-8: Additional VirtIO devices
  * IRQ 10: UART (NS16550a)

**Priority Levels:**

- **0**: Interrupt disabled
- **1-7**: Interrupt enabled (7 = highest priority)

**Key Functions:**

.. code-block:: c

   void plic_init(void);
   void plic_set_priority(uint32_t irq_number, uint32_t priority);
   void plic_enable_interrupt(uint32_t irq_number, uint32_t context);
   uint32_t plic_claim_interrupt(uint32_t context);
   void plic_complete_interrupt(uint32_t irq_number, uint32_t context);

**Memory Map (QEMU virt):**

* Base address: ``0x0C000000``
* Priority registers: ``0x0C000000 + (irq * 4)``
* Enable registers: ``0x0C002000 + (context * 0x80)``
* Claim/complete: ``0x0C200004 + (context * 0x1000)``

CLINT (Core-Local Interruptor)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**File:** ``kernel/arch/riscv64/drivers/clint.c``

**Responsibilities:**

* Timer interrupts for each hart
* Software interrupts between harts
* Provide current time value

**Key Functions:**

.. code-block:: c

   void clint_init(void);
   uint64_t clint_get_time(void);
   void clint_set_timer(uint64_t time_value);
   void clint_enable_timer_interrupt(void);
   void clint_trigger_software_interrupt(uint32_t hart_id);

**Memory Map (QEMU virt):**

* Base address: ``0x02000000``
* Software interrupt: ``0x02000000 + (hart * 4)``
* Timer compare: ``0x02004000 + (hart * 8)``
* Current time: ``0x0200BFF8``

Interrupt Management API
~~~~~~~~~~~~~~~~~~~~~~~~~

**File:** ``kernel/arch/riscv64/drivers/interrupt.c``

**High-level API for kernel and drivers:**

.. code-block:: c

   void interrupt_init(void);
   void interrupt_enable(void);
   void interrupt_disable(void);
   bool interrupt_register_handler(uint32_t irq, interrupt_handler_t handler);
   void interrupt_enable_irq(uint32_t irq);
   void interrupt_set_priority(uint32_t irq, uint32_t priority);

**Handler Registration:**

.. code-block:: c

   // Define a handler function
   void my_device_handler(void) {
       // Handle the interrupt
       hal_uart_puts("Device interrupt!\n");
   }
   
   // Register the handler
   interrupt_register_handler(10, my_device_handler);
   interrupt_set_priority(10, IRQ_PRIORITY_HIGH);
   interrupt_enable_irq(10);

Usage Example
-------------

Basic Timer Interrupt
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "arch/interrupt.h"
   #include "arch/clint.h"
   
   void timer_handler(void) {
       hal_uart_puts("Tick!\n");
       
       // Schedule next interrupt (1 second = 10,000,000 cycles at 10MHz)
       clint_add_timer(10000000);
   }
   
   void setup_timer(void) {
       // Initialize interrupt system
       interrupt_init();
       
       // Enable interrupts globally
       interrupt_enable();
       
       // Configure timer for 1 second intervals
       clint_set_timer(clint_get_time() + 10000000);
       clint_enable_timer_interrupt();
   }

External Device Interrupt
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "arch/interrupt.h"
   
   #define UART_IRQ 10
   
   void uart_interrupt_handler(void) {
       // Handle UART interrupt
       char c = uart_read_char();
       uart_write_char(c);  // Echo
   }
   
   void setup_uart_interrupts(void) {
       // Register handler
       interrupt_register_handler(UART_IRQ, uart_interrupt_handler);
       
       // Set priority
       interrupt_set_priority(UART_IRQ, IRQ_PRIORITY_NORMAL);
       
       // Enable the interrupt
       interrupt_enable_irq(UART_IRQ);
   }

Interrupt Flow
--------------

1. **Interrupt Occurs**
   
   - Hardware sets pending bit in PLIC or CLINT
   - CPU receives interrupt signal
   - CPU jumps to trap vector (``trap_entry.S``)

2. **Context Save**
   
   - All registers saved to trap frame
   - Exception PC (``sepc``) and status (``sstatus``) saved

3. **Trap Handler**
   
   - ``trap_handler()`` reads ``scause`` register
   - Determines if exception or interrupt
   - For external interrupts, calls ``handle_external_interrupt()``

4. **Interrupt Dispatch**
   
   - ``handle_external_interrupt()`` claims interrupt from PLIC
   - Looks up registered handler in table
   - Calls handler function
   - Completes interrupt in PLIC

5. **Context Restore**
   
   - All registers restored from trap frame
   - ``sret`` instruction returns to interrupted code

Priority Levels
---------------

Seven priority levels are available:

* ``IRQ_PRIORITY_DISABLED`` (0) - Interrupt disabled
* ``IRQ_PRIORITY_LOWEST`` (1) - Lowest priority
* ``IRQ_PRIORITY_LOW`` (2)
* ``IRQ_PRIORITY_NORMAL`` (3) - Default
* ``IRQ_PRIORITY_HIGH`` (5)
* ``IRQ_PRIORITY_HIGHEST`` (7) - Highest priority

Higher priority interrupts preempt lower priority ones.

Code Quality Standards
----------------------

The interrupt handling code follows ThunderOS coding standards:

* **Descriptive names**: ``interrupt_handler_t``, ``plic_claim_interrupt()``
* **No magic numbers**: All hardware addresses are named constants
* **Static helpers**: Internal functions marked ``static``
* **Modular functions**: Each function has single responsibility
* **Initialized variables**: All variables explicitly initialized
* **Clear organization**: Constants, forward declarations, implementations

Testing
-------

To test the interrupt system:

1. **Build and run:**

   .. code-block:: bash

      make clean && make all
      make qemu

2. **Verify output:**

   .. code-block:: text

      [OK] Interrupt subsystem initialized
      [OK] Trap handler initialized
      [OK] Interrupts enabled
      [OK] Timer interrupts enabled

3. **Observe timer interrupts:**

   Timer should trigger every second, demonstrating interrupt handling works.

References
----------

* RISC-V Privileged Specification v1.12
* QEMU RISC-V virt machine documentation
* SiFive PLIC specification
* :doc:`../development/code_quality` - ThunderOS coding standards

See Also
--------

* :doc:`timer` - Timer driver documentation
* :doc:`../architecture` - System architecture
* :doc:`../internals/index` - Internal implementation details
