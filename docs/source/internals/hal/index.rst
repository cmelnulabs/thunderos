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
     - 🔄 Planned
     - Next migration target
   * - Interrupts
     - 🔄 Planned
     - After timer
   * - CPU Control
     - 🔄 Planned
     - CSR access, power management
   * - MMU
     - 🔄 Planned
     - Most complex subsystem

Files Updated
~~~~~~~~~~~~~

**HAL Interface:**
   * ``include/hal/hal_uart.h`` - Interface definition

**RISC-V Implementation:**
   * ``kernel/arch/riscv64/drivers/uart.c`` - NS16550A driver

**Portable Kernel Code:**
   * ``kernel/main.c`` - Uses HAL UART
   * ``kernel/arch/riscv64/trap.c`` - Uses HAL for error messages
   * ``kernel/drivers/clint.c`` - Uses HAL for timer output

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

1. **hal_timer.h** - Timer/CLINT abstraction
   
   * ``hal_timer_init()``
   * ``hal_timer_set()``
   * ``hal_timer_get_ticks()``
   * ``hal_timer_delay()``

2. **hal_interrupt.h** - Interrupt/trap handling
   
   * ``hal_interrupt_init()``
   * ``hal_interrupt_enable()``
   * ``hal_interrupt_disable()``
   * ``hal_trap_register()``

3. **hal_cpu.h** - CPU control
   
   * ``hal_cpu_halt()``
   * ``hal_cpu_relax()``
   * ``hal_cpu_id()``
   * CSR access functions (RISC-V specific)

4. **hal_mmu.h** - Memory management unit
   
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

* :doc:`../uart_driver` - Original UART implementation (pre-HAL)
* :doc:`../trap_handler` - Interrupt handling
* :doc:`../timer_clint` - Timer implementation
* :doc:`../../riscv/index` - RISC-V architecture reference
