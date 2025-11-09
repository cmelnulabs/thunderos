Kernel Internals
================

This section documents the internal implementation details of ThunderOS.

.. toctree::
   :maxdepth: 2
   :caption: Components:

   bootloader
   uart_driver
   trap_handler
   interrupt_handling
   syscalls
   hal_timer
   pmm
   kmalloc
   paging
   kstring
   process_management
   user_mode
   testing_framework
   linker_script
   memory_layout
   registers
   hal/index

Overview
--------

ThunderOS is implemented in a combination of:

* **RISC-V Assembly**: Bootloader and low-level initialization
* **C**: Kernel core and drivers
* **Linker Scripts**: Memory layout definition

The following pages provide detailed technical documentation of each component.

Component Status
----------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Component
     - Status
     - Description
   * - :doc:`bootloader`
     - ✓ Done
     - Assembly entry point, stack setup, BSS clearing
   * - :doc:`uart_driver`
     - ✓ Done
     - NS16550A UART driver for serial I/O
   * - :doc:`trap_handler`
     - ✓ Done
     - Exception and interrupt handling infrastructure
   * - :doc:`interrupt_handling`
     - ✓ Done
     - PLIC, CLINT drivers and interrupt management
   * - :doc:`hal_timer`
     - ✓ Done
     - Hardware abstraction layer for timer (portable interface)
   * - :doc:`pmm`
     - ✓ Done
     - Physical memory manager with bitmap allocator
   * - :doc:`kmalloc`
     - ✓ Done
     - Kernel heap allocator with multi-page support
   * - :doc:`kstring`
     - ✓ Done
     - Kernel string utilities (kprint_dec, kprint_hex, kmemcpy, etc.)
   * - :doc:`paging`
     - ✓ Done
     - Virtual memory with Sv39 paging (identity mapping)
   * - :doc:`process_management`
     - ✓ Done
     - Process control blocks, scheduler, context switching
   * - :doc:`user_mode`
     - ✓ Done
     - User mode support with privilege transitions and memory isolation
   * - :doc:`testing_framework`
     - ✓ Done
     - KUnit-inspired testing framework for kernel
   * - :doc:`linker_script`
     - ✓ Done
     - Memory layout and section placement
   * - Syscall Interface
     - In Progress
     - Basic syscall infrastructure (v0.2.0)
   * - Higher-Half Kernel
     - Planned
     - Move kernel to 0xFFFFFFFF80000000 (v0.2.0+)
   * - Fork/Exec
     - Planned
     - Process cloning and program loading (v0.6.0)

Code Organization
-----------------

Source Files
~~~~~~~~~~~~

.. code-block:: text

   boot/
   └── boot.S              # Bootloader assembly

   kernel/
   ├── main.c              # Kernel entry point
   ├── arch/riscv64/
   │   ├── kernel.ld       # Linker script
   │   ├── trap_entry.S    # Assembly trap vector
   │   ├── boot/
   │   ├── core/
   │   │   └── trap.c      # C trap handler
   │   ├── cpu/
   │   ├── drivers/
   │   │   ├── uart.c      # UART HAL implementation
   │   │   └── timer.c     # Timer HAL implementation
   │   └── interrupt/
   ├── core/
   │   ├── panic.c         # Kernel panic handler
   │   ├── process.c       # Process management
   │   ├── scheduler.c     # Process scheduler
   │   ├── syscall.c       # System call handler
   │   └── time.c          # Time management
   ├── utils/
   │   └── kstring.c       # String utilities
   └── mm/
       ├── pmm.c           # Physical memory manager
       ├── kmalloc.c       # Kernel heap allocator
       └── paging.c        # Virtual memory management

   include/
   ├── trap.h              # Trap structures and constants
   ├── hal/
   │   ├── hal_uart.h      # UART HAL interface
   │   └── hal_timer.h     # Timer HAL interface
   ├── kernel/
   │   └── kstring.h       # String utilities interface
   └── mm/
       ├── pmm.h           # PMM interface
       ├── kmalloc.h       # kmalloc interface
       └── paging.h        # Paging interface
   
   tests/
   ├── framework/
   │   ├── kunit.h         # Test framework header
   │   └── kunit.c         # Test framework implementation
   ├── test_trap.c         # Trap handler tests
   ├── test_timer.c        # Timer interrupt tests
   └── Makefile            # Test build system


Build Process
-------------

ThunderOS uses a Makefile-based build system. The build follows these steps:

1. **Compile Assembly Sources**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d -mcmodel=medany \
          -nostdlib -nostartfiles -ffreestanding -fno-common -O0 -Wall -Wextra \
          -Iinclude -c boot/boot.S -o build/boot/boot.o

2. **Compile C Sources**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d -mcmodel=medany \
          -nostdlib -nostartfiles -ffreestanding -fno-common -O0 -Wall -Wextra \
          -Iinclude -c kernel/main.c -o build/kernel/main.o

3. **Link All Objects**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-ld -nostdlib -T kernel/arch/riscv64/kernel.ld \
          -o build/thunderos.elf build/boot/*.o build/kernel/**/*.o

4. **Generate Binary**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-objcopy -O binary build/thunderos.elf build/thunderos.bin

**Quick Build:**

.. code-block:: bash

   make              # Build everything
   make qemu         # Build and run in QEMU
   make debug        # Build and run with GDB server
   make dump         # Generate disassembly
   make clean        # Clean build artifacts

Compiler Flags
~~~~~~~~~~~~~~

.. code-block:: make

   CFLAGS = -march=rv64gc      # RISC-V 64-bit with G (general) + C (compressed) extensions
            -mabi=lp64d        # LP64 ABI with hardware double-precision float registers
            -mcmodel=medany    # Position-independent code model for any address range
            -nostdlib          # Don't link against standard C library
            -nostartfiles      # Don't use standard system startup files
            -ffreestanding     # Kernel runs without hosted environment (no OS beneath it)
            -fno-common        # Place uninitialized globals in BSS (not common blocks)
            -O0                # No optimization (for debugging, preserves code structure)
            -Wall -Wextra      # Enable all common warnings and extra checks
            -Iinclude          # Add include/ directory to header search path

**Flag Details:**

* **-march=rv64gc**: Target RISC-V 64-bit with standard extensions (Integer, Multiply, Atomic, Float, Double, Compressed)
* **-mabi=lp64d**: Long and pointers are 64-bit, doubles in FP registers
* **-mcmodel=medany**: Code can be loaded anywhere in memory (required for kernel)
* **-nostdlib -nostartfiles**: We provide our own entry point and runtime (no libc)
* **-ffreestanding**: Compiler knows we're writing a kernel (no standard library assumptions)
* **-fno-common**: Ensures proper BSS section initialization
* **-O0**: No optimization - keeps debugging easier and code predictable
* **-Wall -Wextra**: Catch common bugs and code issues at compile time



Disassembly
~~~~~~~~~~~

Generate full disassembly:

.. code-block:: bash

   make dump
   # Creates build/thunderos.dump

View specific sections:

.. code-block:: bash

   riscv64-unknown-elf-objdump -d -S build/thunderos.elf | less

Testing
-------

ThunderOS includes a built-in KUnit-inspired testing framework for kernel components:

**Test Framework:**
   * Located in ``tests/framework/`` (kunit.h, kunit.c)
   * Provides assertions, test registration, and execution
   * See :doc:`testing_framework` for details

**Current Tests:**
   * ``tests/test_trap.c`` - Trap handler tests
   * ``tests/test_timer.c`` - Timer interrupt tests
   * Separate test Makefile in ``tests/``

**Running Tests:**

.. code-block:: bash

   cd tests/
   make              # Build test suite
   make qemu         # Run tests in QEMU

**Manual Testing:**
   * QEMU execution with serial output verification
   * Visual verification of boot messages
   * Basic smoke tests (boot sequence, UART output, timer interrupts)

See individual component pages for detailed technical documentation.
