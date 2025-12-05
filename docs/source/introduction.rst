Introduction
============

ThunderOS is a lightweight, educational operating system designed for the RISC-V 64-bit 
architecture, providing a clean foundation for learning OS development.

What is ThunderOS?
------------------

ThunderOS is built from the ground up to explore:

* **RISC-V Architecture**: Understanding the modern, open-source instruction set
* **Operating System Fundamentals**: Memory management, process scheduling, and I/O
* **Systems Programming**: Low-level kernel development in C and Assembly
* **Hardware-Software Integration**: Device drivers, interrupts, and hardware abstraction

Why RISC-V?
-----------

RISC-V is an open standard instruction set architecture (ISA) that offers several advantages:

**Open and Free**
   No licensing fees, anyone can implement RISC-V processors

**Modular Design**
   Base ISA + optional extensions (M, A, F, D, C, V, etc.)

**Clean Architecture**
   Modern design without legacy baggage from x86 or ARM

**Educational Value**
   Simpler to understand than proprietary architectures
   Excellent tooling and emulation support (QEMU)

Target Audience
---------------

This project is designed for:

* Computer science students learning OS development
* Systems programmers interested in RISC-V
* Embedded systems engineers exploring new platforms
* Hobbyists building custom RISC-V systems

Development Philosophy
----------------------

**Simplicity First**
   Start with minimal, working implementations

**Well Documented**
   Every component should be understandable

**Incremental Progress**
   Build features step-by-step: bootloader → interrupts → memory → processes

**Practical Learning**
   Real code running on real (emulated) hardware

Getting Started
---------------

Prerequisites
~~~~~~~~~~~~~

* RISC-V GCC toolchain (``riscv64-unknown-elf-gcc``)
* QEMU 10.1.2+ with RISC-V support (``qemu-system-riscv64``)
* GNU Make
* Basic knowledge of C and assembly

Building
~~~~~~~~

Using the build script (recommended):

.. code-block:: bash

   git clone <repository>
   cd thunderos
   ./build_os.sh

Or manually with Make:

.. code-block:: bash

   make all

Running
~~~~~~~

Build and test in QEMU:

.. code-block:: bash

   make qemu

Or run automated tests:

.. code-block:: bash

   # Run full test suite
   make test
   
   # Or run individual test scripts
   tests/scripts/test_boot.sh
   tests/scripts/test_integration.sh
   tests/scripts/test_user_mode.sh
   
   # Or run all tests
   tests/scripts/run_all_tests.sh

Building Documentation
~~~~~~~~~~~~~~~~~~~~~~

Generate HTML documentation:

.. code-block:: bash

   ./build_docs.sh

Documentation will be available at ``docs/build/html/index.html``

Project Structure
-----------------

.. code-block:: text

   thunderos/
   ├── boot/              # Bootloader (assembly)
   ├── kernel/            # Kernel code
   │   ├── arch/riscv64/  # Architecture-specific code
   │   ├── core/          # Core kernel (scheduler, syscalls, etc.)
   │   └── mm/            # Memory management (PMM, paging)
   ├── include/           # Header files
   │   ├── arch/          # Architecture headers
   │   ├── hal/           # Hardware Abstraction Layer
   │   ├── kernel/        # Kernel subsystem headers
   │   └── mm/            # Memory management headers
   ├── tests/             # Automated test suite
   │   ├── test_*.sh      # Integration test scripts
   │   ├── test_*.c       # Unit test programs
   │   ├── framework/     # Test framework (kunit)
   │   └── README.md      # Test documentation
   ├── docs/              # Sphinx documentation
   ├── build/             # Build artifacts (generated)
   ├── .github/           # GitHub Actions CI/CD
   ├── build_os.sh        # Build kernel script
   ├── build_docs.sh      # Build documentation script
   ├── Makefile           # Main build system
   └── README.md          # Project documentation

Next Steps
----------

* Read the :doc:`architecture` overview
* Dive into :doc:`internals/index` for implementation details
* Check :doc:`development` for contribution guidelines
