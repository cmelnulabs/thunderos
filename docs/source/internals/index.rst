Kernel Internals
================

This section documents the internal implementation details of ThunderOS.

.. toctree::
   :maxdepth: 2
   :caption: Components:

   bootloader
   uart_driver
   trap_handler
   timer_clint
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
   * - :doc:`timer_clint`
     - ✓ Done
     - CLINT timer driver using SBI for timer interrupts
   * - :doc:`testing_framework`
     - ✓ Done
     - KUnit-inspired testing framework for kernel
   * - :doc:`linker_script`
     - ✓ Done
     - Memory layout and section placement
   * - Memory Management
     - TODO
     - Physical allocator, paging, heap
   * - Process Scheduler
     - TODO
     - Task structures, context switching

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
   │   └── trap.c          # C trap handler
   ├── core/               # (Future) Core kernel
   ├── drivers/
   │   ├── uart.c          # UART driver
   │   └── clint.c         # CLINT timer driver
   └── mm/                 # (Future) Memory management

   include/
   ├── uart.h              # UART interface
   ├── trap.h              # Trap structures and constants
   └── clint.h             # Timer interface
   
   tests/
   ├── framework/
   │   ├── kunit.h         # Test framework header
   │   └── kunit.c         # Test framework implementation
   ├── test_trap.c         # Trap handler tests
   ├── test_timer.c        # Timer interrupt tests
   └── Makefile            # Test build system

Coding Conventions
~~~~~~~~~~~~~~~~~~

**C Style**
   * Use descriptive variable names
   * Comment non-obvious code
   * Keep functions small and focused
   * No standard library (freestanding)

**Assembly Style**
   * Use comments for each logical block
   * Label jump targets clearly
   * Prefer pseudo-instructions when clear

**Naming**
   * Functions: ``lowercase_with_underscores()``
   * Macros/Constants: ``UPPERCASE_WITH_UNDERSCORES``
   * Types: ``PascalCase`` or ``lowercase_t``

Build Process
-------------

The build follows these steps:

1. **Compile Assembly**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-gcc -c boot/boot.S -o build/boot/boot.o

2. **Compile C Sources**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-gcc -c kernel/main.c -o build/kernel/main.o

3. **Link**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-ld -T kernel.ld -o thunderos.elf *.o

4. **Generate Binary**
   
   .. code-block:: bash
   
      riscv64-unknown-elf-objcopy -O binary thunderos.elf thunderos.bin

Compiler Flags
~~~~~~~~~~~~~~

.. code-block:: make

   CFLAGS = -march=rv64gc      # RISC-V 64-bit with common extensions
            -mabi=lp64d        # LP64 ABI with double-precision float
            -mcmodel=medany    # Medium any code model (position-independent)
            -nostdlib          # No standard library
            -nostartfiles      # No standard startup files
            -ffreestanding     # Freestanding environment (no hosted features)
            -fno-common        # No common blocks
            -O2                # Optimization level 2
            -Wall -Wextra      # All warnings

Debugging
---------

QEMU with GDB
~~~~~~~~~~~~~

Start QEMU with GDB server:

.. code-block:: bash

   make debug
   # Equivalent to:
   qemu-system-riscv64 ... -s -S

In another terminal:

.. code-block:: bash

   riscv64-unknown-elf-gdb build/thunderos.elf
   (gdb) target remote :1234
   (gdb) break kernel_main
   (gdb) continue

Useful GDB Commands
~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   info registers          # Show all registers
   x/10i $pc              # Disassemble 10 instructions at PC
   x/20x 0x80200000       # Examine 20 words of memory
   stepi                  # Step one instruction
   layout asm             # Show assembly in TUI
   layout regs            # Show registers in TUI

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

Current Testing
~~~~~~~~~~~~~~~

* Manual testing in QEMU
* Visual verification of boot messages
* Basic smoke tests (does it boot? does UART work?)

Future Testing
~~~~~~~~~~~~~~

* Unit tests for kernel functions
* Integration tests for subsystems
* Automated QEMU tests with expect scripts
* Hardware testing on real RISC-V boards

Performance Considerations
--------------------------

Current State
~~~~~~~~~~~~~

Performance is not yet a concern - the kernel does almost nothing.

Future Optimizations
~~~~~~~~~~~~~~~~~~~~

* **Code Placement**: Hot code in same cache lines
* **Branch Prediction**: Hint likely branches
* **Vector Instructions**: Use RVV for data parallel operations
* **Memory Alignment**: Align data structures to cache lines
* **Instruction Selection**: Profile and optimize critical paths

See individual component pages for detailed technical documentation.
