ThunderOS Documentation
=======================

**ThunderOS** is a RISC-V operating system specialized for AI workloads. 
It is designed from scratch to take advantage of RISC-V's open architecture 
and extensibility, particularly the Vector Extension (RVV) for AI acceleration.

**License**: GNU General Public License v3.0 (GPL v3)

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   introduction
   architecture
   internals/index
   riscv/index
   development
   api

Project Goals
-------------

* Educational OS implementation in RISC-V
* Optimized for AI workload scheduling
* Support for RISC-V Vector Extension (RVV)
* Clean, well-documented codebase
* Hardware accelerator integration

Quick Start
-----------

Build the kernel::

    make all

Run in QEMU::

    make qemu

Current Status
--------------

.. list-table::
   :header-rows: 1

   * - Component
     - Status
   * - Bootloader
     - ✓ Implemented
   * - UART Driver
     - ✓ Implemented
   * - Trap Handler
     - ✓ Implemented
   * - Timer Interrupts (CLINT)
     - ✓ Implemented
   * - Testing Framework
     - ✓ Implemented
   * - Memory Management
     - ⏳ TODO
   * - Process Scheduler
     - ⏳ TODO
   * - AI Accelerators
     - ⏳ TODO

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

License
=======

ThunderOS is free software licensed under the **GNU General Public License v3.0 (GPL v3)**.

This means you are free to:

* Use the software for any purpose
* Study how it works and modify it
* Distribute copies
* Distribute modified versions

Under the following conditions:

* Source code must be made available when distributing the software
* Modified versions must also be licensed under GPL v3
* Changes must be documented

See the full LICENSE file in the repository for complete terms and conditions.

**Copyright © 2025 ThunderOS Team**
