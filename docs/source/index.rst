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

**Version 0.2.0 - "User Space"** ✅ RELEASED

ThunderOS v0.2.0 "User Space" is now complete with full user-mode support!

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
   * - Memory Management (PMM + kmalloc)
     - ✓ Implemented
   * - Virtual Memory (Sv39 paging)
     - ✓ Implemented
   * - Process Scheduler
     - ✓ Implemented
   * - User Mode (U-mode) Support
     - ✓ Implemented
   * - System Calls (13 syscalls)
     - ✓ Implemented
   * - Privilege Separation
     - ✓ Implemented
   * - Exception Handling
     - ✓ Implemented
   * - Automated Testing Framework
     - ✓ Implemented
   * - CI/CD Pipeline (GitHub Actions)
     - ✓ Implemented
   * - AI Accelerators
     - ⏳ TODO (v0.3.0)

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
