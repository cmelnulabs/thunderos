ThunderOS Documentation
=======================

**ThunderOS** is a RISC-V operating system specialized for AI workloads. 
It is designed from scratch to take advantage of RISC-V's open architecture 
and extensibility, particularly the Vector Extension (RVV) for AI acceleration.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   introduction
   architecture
   internals/index
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
