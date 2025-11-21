System Architecture
===================

ThunderOS follows a monolithic kernel architecture with modular components.

High-Level Overview
-------------------

.. code-block:: text

                    ┌─────────────────────────────┐
                    │   User Space (v0.2.0+)      │
                    │  - ELF binaries          ✓  │
                    │  - Signal handling       ✓  │
                    │  - Memory isolation      ✓  │
                    └─────────────────────────────┘
                               │ syscalls
                    ┌──────────▼──────────────────┐
                    │      Kernel Space           │
                    │  ┌──────────────────────┐   │
                    │  │   Process Scheduler  │   │
                    │  │  - Round-robin    ✓  │   │
                    │  │  - Time slicing   ✓  │   │
                    │  │  - Preemptive     ✓  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │  Memory Management   │   │
                    │  │  - PMM (bitmap)   ✓  │   │
                    │  │  - kmalloc        ✓  │   │
                    │  │  - Sv39 paging    ✓  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │   Interrupt System   │   │
                    │  │  - Trap Handler   ✓  │   │
                    │  │  - Timer (CLINT)  ✓  │   │
                    │  │  - PLIC           ✓  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │   Device Drivers     │   │
                    │  │   - UART (HAL)    ✓  │   │
                    │  │   - Timer (HAL)   ✓  │   │
                    │  │   - VirtIO Block  ✓  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │   Filesystems        │   │
                    │  │   - VFS Layer     ✓  │   │
                    │  │   - ext2          ✓  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │   Testing Framework  │   │
                    │  │   - KUnit-style   ✓  │   │
                    │  └──────────────────────┘   │
                    └─────────────────────────────┘
                               │
                    ┌──────────▼──────────────────┐
                    │     Hardware (QEMU)         │
                    │  - RISC-V 64-bit CPU        │
                    │  - Memory (128MB)           │
                    │  - UART (NS16550A)          │
                    │  - PLIC/CLINT               │
                    └─────────────────────────────┘

Boot Process
------------

The boot sequence follows this flow:

1. **Power On / Reset**
   
   * CPU starts at reset vector (implementation-dependent)
   * In QEMU virt machine: 0x1000

2. **Firmware Stage (OpenSBI)**
   
   * OpenSBI loads at 0x80000000
   * Initializes hardware (UART, timers, interrupts)
   * Provides SBI (Supervisor Binary Interface) services
   * Runs in M-mode (Machine mode - highest privilege)

3. **Bootloader (boot.S)**
   
   * Loads at 0x80200000
   * Entry point: ``_start``
   * Runs in S-mode (Supervisor mode)
   * Responsibilities:
   
     * Disable interrupts
     * Setup stack pointer
     * Clear BSS section
     * Jump to C kernel

4. **Kernel Initialization (kernel_main)**
   
   * Initialize UART
   * Setup trap handler
   * Initialize timer (CLINT)
   * Enable interrupts
   * Print boot messages
   * Enter idle loop (WFI - Wait For Interrupt)

Memory Layout
-------------

ThunderOS uses the following memory map on QEMU virt machine:

.. code-block:: text

   ┌─────────────────────┬──────────────┬─────────────────────┐
   │ Address Range       │ Size         │ Description         │
   ├─────────────────────┼──────────────┼─────────────────────┤
   │ 0x00001000          │ 4KB          │ Boot ROM            │
   │ 0x02000000          │ 16KB         │ CLINT (timer)       │
   │ 0x0C000000          │ 32MB         │ PLIC (interrupts)   │
   │ 0x10000000          │ 256B         │ UART0               │
   │ 0x80000000          │ 128KB        │ OpenSBI (firmware)  │
   │ 0x80200000          │ ~1MB         │ ThunderOS Kernel    │
   │   ├─ .text          │              │   Code segment      │
   │   ├─ .rodata        │              │   Read-only data    │
   │   ├─ .data          │              │   Initialized data  │
   │   └─ .bss           │              │   Uninitialized     │
   │ 0x87000000          │ ~120MB       │ Free RAM            │
   └─────────────────────┴──────────────┴─────────────────────┘

Privilege Levels
----------------

RISC-V defines multiple privilege levels:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Level
     - Name
     - Usage
   * - 0 (U)
     - User
     - Application code (future)
   * - 1 (S)
     - Supervisor
     - **ThunderOS kernel runs here**
   * - 3 (M)
     - Machine
     - OpenSBI firmware

ThunderOS runs in **S-mode** and relies on OpenSBI (M-mode) for:

* Timer interrupts via SBI calls
* Console I/O (early boot)
* System reset/shutdown

Key Components
--------------

Bootloader
~~~~~~~~~~

* **File**: ``boot/boot.S``
* **Purpose**: First code executed after firmware
* **Language**: RISC-V assembly
* **Status**: ✓ Implemented
* Responsibilities:

  * Disable interrupts
  * Environment setup (stack, BSS)
  * Transfer control to C code

See :doc:`internals/bootloader` for details.

UART Driver
~~~~~~~~~~~

* **Files**: ``kernel/drivers/uart.c``, ``include/uart.h``
* **Purpose**: Serial console I/O
* **Hardware**: NS16550A compatible UART
* **Status**: ✓ Implemented
* Functions:

  * ``uart_init()`` - Initialize driver
  * ``uart_putc()`` - Output character
  * ``uart_puts()`` - Output string
  * ``uart_getc()`` - Input character

See :doc:`internals/uart_driver` for details.

Trap Handler
~~~~~~~~~~~~

* **Files**: ``kernel/arch/riscv64/trap_entry.S``, ``kernel/arch/riscv64/trap.c``, ``include/trap.h``
* **Purpose**: Handle exceptions and interrupts
* **Status**: ✓ Implemented
* Components:

  * ``trap_vector`` - Assembly entry point (saves/restores context)
  * ``trap_handler()`` - C handler (dispatches by cause)
  * ``handle_exception()`` - Exception handler
  * ``handle_interrupt()`` - Interrupt dispatcher

Features:

* Complete register save/restore (34 registers)
* Exception identification and reporting
* Interrupt routing (timer, software, external)
* Integration with timer driver

See :doc:`internals/trap_handler` for details.

Timer Driver (CLINT)
~~~~~~~~~~~~~~~~~~~~

* **Files**: ``kernel/drivers/clint.c``, ``include/clint.h``
* **Purpose**: Periodic timer interrupts for timekeeping
* **Hardware**: CLINT (Core Local Interruptor) via SBI
* **Status**: ✓ Implemented
* Functions:

  * ``clint_init()`` - Initialize timer and enable interrupts
  * ``clint_get_ticks()`` - Get tick counter
  * ``clint_set_timer()`` - Schedule next interrupt
  * ``clint_handle_timer()`` - Interrupt handler

Features:

* SBI-based timer programming (S-mode compatible)
* Configurable interrupt interval (default: 1 second)
* Tick counter for timekeeping
* Uses ``rdtime`` instruction for precise timing

See :doc:`internals/hal_timer` for details.

Testing Framework
~~~~~~~~~~~~~~~~~

* **Files**: ``tests/framework/kunit.{c,h}``
* **Purpose**: Automated kernel testing
* **Status**: ✓ Implemented
Features:

* KUnit-inspired API
* Assertion macros (EXPECT_EQ, EXPECT_NE, etc.)
* Test suite organization
* Formatted TAP-style output
* Kernel-embedded unit tests

Test Suites:

* ``tests/unit/test_memory_mgmt.c`` - Memory management tests (10 tests)
* ``tests/unit/test_memory_isolation.c`` - Process isolation tests (15 tests)
* ``tests/unit/test_elf.c`` - ELF loader tests (8 tests)
* ``tests/scripts/run_all_tests.sh`` - Integration test runner

See :doc:`internals/testing_framework` for details.

Kernel Main
~~~~~~~~~~~

* **File**: ``kernel/main.c``
* **Purpose**: Main kernel entry point
* **Current functionality**:

  * Initialize UART
  * Setup trap handler
  * Initialize timer
  * Print boot messages
  * Idle loop with WFI (Wait For Interrupt)

Build System
------------

The build process uses GNU Make:

.. code-block:: make

   # Toolchain
   CC = riscv64-unknown-elf-gcc
   LD = riscv64-unknown-elf-ld
   
   # Flags
   CFLAGS = -march=rv64gc -mabi=lp64d -ffreestanding
   LDFLAGS = -T kernel/arch/riscv64/kernel.ld

Build targets:

* ``make all`` - Build kernel ELF and binary
* ``make clean`` - Remove build artifacts
* ``make qemu`` - Run kernel in QEMU
* ``make debug`` - Run with GDB server
* ``make dump`` - Generate disassembly

QEMU Configuration
------------------

ThunderOS requires QEMU 10.1.2+ and targets the ``virt`` machine:

.. code-block:: bash

   qemu-system-riscv64 \
     -machine virt \      # Generic virtual RISC-V board
     -m 128M \            # 128MB RAM
     -nographic \         # No GUI, use terminal
     -serial mon:stdio \  # Serial to stdout
     -bios none \         # No firmware (kernel includes M-mode code)
     -kernel thunderos.elf

The ``virt`` machine provides:

* 1 RISC-V CPU (configurable)
* RAM at 0x80000000
* UART at 0x10000000
* PLIC at 0x0C000000
* ACLINT at 0x02000000
* VirtIO devices (block storage, etc.)

Future Architecture
-------------------

ThunderOS is under active development. For detailed information about planned features and the development roadmap, see:

* `ROADMAP.md <../../ROADMAP.md>`_ - Complete development roadmap from v0.1 to v2.0
* `CHANGELOG.md <../../CHANGELOG.md>`_ - Detailed history of implemented features

See :doc:`internals/index` for detailed implementation documentation of current features.
