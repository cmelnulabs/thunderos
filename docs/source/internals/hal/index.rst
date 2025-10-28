Hardware Abstraction Layer (HAL)
=================================

Overview
--------

The Hardware Abstraction Layer (HAL) provides a clean interface between portable kernel code and architecture-specific implementations.

**Design Goals:**

* **Portability**: Write kernel features once, run on any architecture
* **RISC-V First**: All development starts with RISC-V, optimizations preserved
* **Zero Overhead**: HAL uses static inline functions where possible
* **Clean Separation**: Architecture assumptions isolated from portable code

Current Status
--------------

**UART HAL - ✅ Implemented**

The UART subsystem has been fully migrated to HAL.

**Timer HAL - ✅ Implemented**

The timer subsystem has been fully migrated to HAL, providing portable timer interfaces.

Architecture Support
--------------------

**RISC-V 64-bit** (Primary - Complete)
   * NS16550A UART driver for QEMU virt machine
   * Full implementation in ``kernel/arch/riscv64/drivers/uart.c``

**ARM64/AArch64** (Planned)
   * PL011 UART driver (future)
   * Will implement same HAL interface

**x86-64** (Planned)
   * 16550 UART driver (future)
   * Will implement same HAL interface

HAL UART Interface
------------------

Located in ``include/hal/hal_uart.h``

Functions
~~~~~~~~~

.. code-block:: c

   void hal_uart_init(void);
   void hal_uart_putc(char c);
   void hal_uart_puts(const char *s);
   char hal_uart_getc(void);

**hal_uart_init()**
   Initialize UART hardware. Architecture-specific implementation configures
   baud rate, data bits, stop bits, and parity.

**hal_uart_putc(char c)**
   Write a single character to UART. Blocks until character is transmitted.

**hal_uart_puts(const char \*s)**
   Write a null-terminated string to UART. Handles newline conversion (\\n → \\r\\n)
   internally for terminal compatibility.

**hal_uart_getc()**
   Read a single character from UART. Blocks until character is available.

RISC-V Implementation
~~~~~~~~~~~~~~~~~~~~~

File: ``kernel/arch/riscv64/drivers/uart.c``

The RISC-V implementation uses the NS16550A UART found in QEMU's virt machine:

* **Base Address**: ``0x10000000``
* **Registers**: RBR (read), THR (write), LSR (status)
* **Initialization**: Minimal (OpenSBI pre-configures UART)
* **Newline Handling**: Converts ``\n`` to ``\r\n`` for terminal compatibility

Example Usage
~~~~~~~~~~~~~

Portable kernel code:

.. code-block:: c

   #include "hal/hal_uart.h"

   void kernel_main(void) {
       hal_uart_init();
       hal_uart_puts("ThunderOS booting...\n");
       
       char c = hal_uart_getc();
       hal_uart_putc(c);  // Echo
   }

This code works unchanged on RISC-V, ARM64, or x86-64!

HAL Timer Interface
-------------------

Located in ``include/hal/hal_timer.h``

See :doc:`../hal_timer` for complete documentation.

Functions
~~~~~~~~~

.. code-block:: c

   void hal_timer_init(unsigned long interval_us);
   unsigned long hal_timer_get_ticks(void);
   void hal_timer_set_next(unsigned long interval_us);
   void hal_timer_handle_interrupt(void);

**hal_timer_init(interval_us)**
   Initialize timer hardware and start periodic interrupts at specified interval
   (in microseconds). Enables timer interrupts and global interrupts.

**hal_timer_get_ticks()**
   Returns the number of timer interrupts that have occurred since initialization.
   Useful for timekeeping and scheduling decisions.

**hal_timer_set_next(interval_us)**
   Schedule the next timer interrupt to occur after the specified number of
   microseconds. Typically called by ``hal_timer_handle_interrupt()``.

**hal_timer_handle_interrupt()**
   Called by the trap handler when a timer interrupt occurs. Increments tick
   counter and schedules the next interrupt automatically.

RISC-V Timer Implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

File: ``kernel/arch/riscv64/drivers/timer.c``

The RISC-V implementation uses the CLINT (Core Local Interruptor) accessed via SBI:

* **Timer Frequency**: 10 MHz on QEMU virt machine
* **Interface**: SBI ecall for setting timer comparator
* **Reading Time**: ``rdtime`` CSR instruction
* **Interrupt Enable**: STIE bit in ``sie`` CSR
* **Automatic Re-scheduling**: Each interrupt schedules the next

Example Usage
~~~~~~~~~~~~~

Portable kernel code:

.. code-block:: c

   #include "hal/hal_timer.h"

   void kernel_main(void) {
       hal_uart_init();
       trap_init();
       
       // Start timer: 1-second intervals
       hal_timer_init(1000000);
       
       // Idle loop - interrupts will fire
       while (1) {
           asm volatile("wfi");  // Wait for interrupt
       }
   }

**Output:**

.. code-block:: text

   Timer initialized (interval: 1 second)
   Tick: 1
   Tick: 2
   Tick: 3
   ...

Migration Status
----------------

**Completed:**

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Subsystem
     - Status
     - Notes
   * - UART
     - ✅ Complete
     - Fully tested on RISC-V QEMU
   * - Timer
     - ✅ Complete
     - RISC-V implementation via SBI/CLINT
   * - Interrupts
     - 🔄 Planned
     - Next migration target
   * - CPU Control
     - 🔄 Planned
     - CSR access, power management
   * - MMU
     - 🔄 Planned
     - Most complex subsystem

Files Updated
~~~~~~~~~~~~~

**HAL Interface:**
   * ``include/hal/hal_uart.h`` - UART interface definition
   * ``include/hal/hal_timer.h`` - Timer interface definition

**RISC-V Implementation:**
   * ``kernel/arch/riscv64/drivers/uart.c`` - NS16550A driver
   * ``kernel/arch/riscv64/drivers/timer.c`` - CLINT/SBI timer driver

**Portable Kernel Code:**
   * ``kernel/main.c`` - Uses HAL UART and HAL Timer
   * ``kernel/arch/riscv64/core/trap.c`` - Uses HAL for error messages and timer handling
   * ``kernel/mm/pmm.c`` - Uses HAL UART for output
   * ``kernel/mm/kmalloc.c`` - Uses HAL UART for error messages

Benefits Achieved
-----------------

**Code Organization**
   Clear separation between interface (``include/hal/``) and implementation
   (``kernel/arch/riscv64/``). RISC-V specific code isolated.

**Portability**
   ``kernel/main.c`` is now architecture-independent. Adding ARM64 support
   requires only implementing the HAL interface for ARM.

**Maintainability**
   Interface documented in one place. Easy to find RISC-V specific code.
   Clean abstraction layer for future development.

**Testing**
   Can develop on x86-64 workstation, test on RISC-V QEMU, deploy on
   RISC-V hardware - same kernel code throughout.

Future HAL Subsystems
----------------------

Planned HAL interfaces (in order of implementation):

1. **hal_interrupt.h** - Interrupt/trap handling
   
   * ``hal_interrupt_init()``
   * ``hal_interrupt_enable()``
   * ``hal_interrupt_disable()``
   * ``hal_trap_register()``

2. **hal_cpu.h** - CPU control
   
   * ``hal_cpu_halt()``
   * ``hal_cpu_relax()``
   * ``hal_cpu_id()``
   * CSR access functions (RISC-V specific)

3. **hal_mmu.h** - Memory management unit
   
   * ``hal_mmu_init()``
   * ``hal_mmu_map()``
   * ``hal_mmu_unmap()``
   * ``hal_tlb_flush()``

Porting Guide
-------------

To port ThunderOS to a new architecture:

1. **Create Architecture Directory**
   
   .. code-block:: text
   
      kernel/arch/<arch>/
      ├── drivers/         # HAL implementations
      ├── interrupt/       # Trap handling
      ├── cpu/             # CPU control
      ├── mm/              # MMU management
      └── kernel.ld        # Linker script

2. **Implement HAL Interfaces**
   
   Start with UART (simplest), then timer, interrupts, CPU, MMU.
   Each HAL interface is documented in ``include/hal/``.

3. **Update Build System**
   
   Modify ``Makefile`` to support ``ARCH=<arch>`` parameter.

4. **Test**
   
   Portable kernel code should work without modification!

See Also
--------

* :doc:`../uart_driver` - NS16550A UART hardware details
* :doc:`../trap_handler` - Interrupt handling
* :doc:`../hal_timer` - Timer HAL interface
* :doc:`../../riscv/index` - RISC-V architecture reference
