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

To build and run ThunderOS, see the :doc:`development` guide for complete instructions 
including prerequisites, build options (Docker or native), and testing.

**Quick Start:**

.. code-block:: bash

   git clone <repository>
   cd thunderos
   make run              # Build and run in QEMU

Project Structure
-----------------

.. code-block:: text

   thunderos/
   ├── boot/              # Bootloader (assembly entry point)
   ├── kernel/            # Kernel source code
   │   ├── arch/riscv64/  # Architecture-specific code
   │   ├── core/          # Core kernel (scheduler, syscalls, signals, etc.)
   │   ├── drivers/       # Device drivers (VirtIO block/net)
   │   ├── fs/            # Filesystem (ext2, VFS)
   │   ├── mm/            # Memory management (PMM, paging, DMA)
   │   └── utils/         # Utility functions (kstring)
   ├── include/           # Header files
   │   ├── arch/          # Architecture headers
   │   ├── drivers/       # Driver headers
   │   ├── fs/            # Filesystem headers
   │   ├── hal/           # Hardware Abstraction Layer
   │   ├── kernel/        # Kernel subsystem headers
   │   └── mm/            # Memory management headers
   ├── userland/          # User-space programs
   ├── tests/             # Automated test suite
   │   ├── framework/     # Test framework (kunit)
   │   ├── scripts/       # Integration test scripts
   │   └── unit/          # Unit test programs
   ├── docs/              # Sphinx documentation
   ├── build/             # Build artifacts (generated)
   ├── .github/           # GitHub Actions CI/CD
   ├── Makefile           # Main build system
   └── README.md          # Project documentation

Next Steps
----------

* Read the :doc:`architecture` overview
* Dive into :doc:`internals/index` for implementation details
* Check :doc:`development` for contribution guidelines
