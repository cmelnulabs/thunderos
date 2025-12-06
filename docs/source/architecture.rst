System Architecture
===================

ThunderOS follows a monolithic kernel architecture with modular components.

High-Level Overview
-------------------

.. code-block:: text

   ╔═══════════════════════════════════════════════════════════════════════════╗
   ║                              USER SPACE                                   ║
   ╠═══════════════════════════════════════════════════════════════════════════╣
   ║  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐       ║
   ║  │    Shell    │  │     ls      │  │    cat      │  │  User Apps  │       ║
   ║  │  (builtin)  │  │   (ELF)     │  │   (ELF)     │  │   (ELF)     │       ║
   ║  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘       ║
   ║         └────────────────┴────────────────┴────────────────┘              ║
   ║                                   │                                       ║
   ║                          System Call Interface                            ║
   ║                    (62 syscalls: read, write, fork, exec...)              ║
   ╠═══════════════════════════════════════════════════════════════════════════╣
   ║                             KERNEL SPACE                                  ║
   ╠═══════════════════════════════════════════════════════════════════════════╣
   ║                                                                           ║
   ║  ┌─────────────────────────────────────────────────────────────────────┐  ║
   ║  │                        CORE SUBSYSTEMS                              │  ║
   ║  ├─────────────────┬─────────────────┬─────────────────────────────────┤  ║
   ║  │   Scheduler     │    Signals      │         Synchronization         │  ║
   ║  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ║
   ║  │  • Preemptive   │  • SIGINT       │  • Mutexes      • Semaphores    │  ║
   ║  │  • Round-robin  │  • SIGKILL      │  • Condvars     • RW Locks      │  ║
   ║  │  • Time slicing │  • SIGCHLD      │  • Wait Queues  • Pipes         │  ║
   ║  └─────────────────┴─────────────────┴─────────────────────────────────┘  ║
   ║                                                                           ║
   ║  ┌─────────────────────────────────────────────────────────────────────┐  ║
   ║  │                      MEMORY MANAGEMENT                              │  ║
   ║  ├───────────────────┬───────────────────┬─────────────────────────────┤  ║
   ║  │   Physical MM     │   Virtual MM      │         Allocators          │  ║
   ║  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ║
   ║  │  • Bitmap alloc   │  • Sv39 paging    │  • kmalloc/kfree            │  ║
   ║  │  • 4KB pages      │  • User/Kernel    │  • DMA allocator            │  ║
   ║  │  • 128MB RAM      │  • Memory isolate │  • Per-process heaps        │  ║
   ║  └───────────────────┴───────────────────┴─────────────────────────────┘  ║
   ║                                                                           ║
   ║  ┌─────────────────────────────────────────────────────────────────────┐  ║
   ║  │                       FILESYSTEMS                                   │  ║
   ║  ├───────────────────────────────┬─────────────────────────────────────┤  ║
   ║  │   Virtual File System (VFS)   │            ext2 Driver              │  ║
   ║  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ║
   ║  │  • Unified file operations    │  • Superblock, inodes, directories  │  ║
   ║  │  • File descriptors           │  • Block allocation (bitmap)        │  ║
   ║  │  • Path resolution            │  • Read/Write/Create/Delete         │  ║
   ║  └───────────────────────────────┴─────────────────────────────────────┘  ║
   ║                                                                           ║
   ║  ┌─────────────────────────────────────────────────────────────────────┐  ║
   ║  │                       DEVICE DRIVERS                                │  ║
   ║  ├─────────────┬─────────────┬─────────────┬─────────────┬─────────────┤  ║
   ║  │    UART     │  VirtIO-BLK │  VirtIO-GPU │ Framebuffer │    Timer    │  ║
   ║  │  (console)  │   (disk)    │  (display)  │  (console)  │   (CLINT)   │  ║
   ║  └─────────────┴─────────────┴─────────────┴─────────────┴─────────────┘  ║
   ║                                                                           ║
   ║  ┌─────────────────────────────────────────────────────────────────────┐  ║
   ║  │                    INTERRUPT HANDLING                               │  ║
   ║  ├───────────────────────────────┬─────────────────────────────────────┤  ║
   ║  │         Trap Handler          │              PLIC                   │  ║
   ║  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │  ║
   ║  │  • Exception dispatch         │  • External interrupt routing       │  ║
   ║  │  • Syscall entry              │  • Priority handling                │  ║
   ║  │  • Context save/restore       │  • UART, VirtIO IRQs                │  ║
   ║  └───────────────────────────────┴─────────────────────────────────────┘  ║
   ║                                                                           ║
   ╠═══════════════════════════════════════════════════════════════════════════╣
   ║                              HARDWARE                                     ║
   ╠═══════════════════════════════════════════════════════════════════════════╣
   ║                                                                           ║
   ║  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐       ║
   ║  │  RISC-V 64  │  │   Memory    │  │   VirtIO    │  │  NS16550A   │       ║
   ║  │    CPU      │  │   128 MB    │  │   MMIO      │  │    UART     │       ║
   ║  │  (RV64GC)   │  │    RAM      │  │  Devices    │  │  Console    │       ║
   ║  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘       ║
   ║                                                                           ║
   ║                         QEMU virt Machine                                 ║
   ╚═══════════════════════════════════════════════════════════════════════════╝

Boot Process
------------

ThunderOS uses a two-stage boot with ``-bios none`` mode (no external firmware):

1. **M-mode entry** (``entry.S``) → hardware init → ``mret`` to S-mode
2. **S-mode boot** (``boot.S``) → BSS clear → ``kernel_main()``
3. **Kernel init** → drivers, memory, filesystem → launch shell

See :doc:`internals/bootloader` for detailed boot flow and implementation.

Memory Layout
-------------

ThunderOS runs on QEMU virt machine with 128MB RAM at ``0x80000000``:

* **Kernel** loads at ``0x80000000`` (~1MB)
* **Free RAM** from ``_kernel_end`` to ``0x88000000`` (~127MB)
* **Device MMIO**: UART at ``0x10000000``, PLIC at ``0x0C000000``, VirtIO at ``0x10001000+``

See :doc:`internals/linker_script` for complete memory map and section layout.

Privilege Levels
----------------

RISC-V defines multiple privilege levels. ThunderOS uses ``-bios none`` mode, 
handling both M-mode initialization and S-mode kernel operation:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Level
     - Name
     - Usage in ThunderOS
   * - 0 (U)
     - User
     - User applications (shell, ls, cat)
   * - 1 (S)
     - Supervisor
     - **Kernel runs here** (after M-mode init)
   * - 3 (M)
     - Machine
     - Early boot only (entry.S → start.c)

See :doc:`riscv/csr_registers` for privilege-related CSR documentation.

Key Components
--------------

For detailed documentation of each kernel subsystem, see :doc:`internals/index`.

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Subsystem
     - Key Files
     - Documentation
   * - Boot
     - ``boot/boot.S``, ``boot/entry.S``
     - :doc:`internals/bootloader`
   * - Interrupts
     - ``kernel/arch/riscv64/trap*.c``
     - :doc:`internals/interrupt_handling`
   * - Timer
     - ``kernel/arch/riscv64/drivers/timer.c``
     - :doc:`internals/hal_timer`
   * - Memory
     - ``kernel/mm/pmm.c``, ``paging.c``, ``kmalloc.c``
     - :doc:`internals/pmm`, :doc:`internals/paging`
   * - Processes
     - ``kernel/core/process.c``, ``scheduler.c``
     - :doc:`internals/process_management`
   * - Filesystem
     - ``kernel/fs/vfs.c``, ``ext2_*.c``
     - :doc:`internals/ext2_filesystem`
   * - Drivers
     - ``kernel/drivers/virtio_*.c``
     - :doc:`internals/virtio_block`
   * - Testing
     - ``tests/framework/kunit.c``
     - :doc:`internals/testing_framework`

Build System
------------

ThunderOS uses GNU Make with a RISC-V cross-compiler toolchain. Key targets:

* ``make run`` - Build and run in QEMU
* ``make test`` - Run automated test suite
* ``make debug`` - Run with GDB server attached

For complete build instructions, prerequisites, and Docker setup, see :doc:`development`.

QEMU Target Platform
--------------------

ThunderOS targets the QEMU ``virt`` machine, a generic RISC-V virtual platform providing:

.. list-table::
   :widths: 30 70

   * - **CPU**
     - RV64GC (1 core, configurable)
   * - **RAM**
     - 128MB at ``0x80000000``
   * - **UART**
     - NS16550A at ``0x10000000``
   * - **Interrupts**
     - PLIC at ``0x0C000000``, CLINT at ``0x02000000``
   * - **Storage**
     - VirtIO block device (ext2 filesystem)
   * - **Display**
     - VirtIO GPU (optional)

See :doc:`development` for QEMU invocation examples and debugging setup.

Future Architecture
-------------------

ThunderOS is under active development. For detailed information about planned features and the development roadmap, see:

* `ROADMAP.md <../../ROADMAP.md>`_ - Complete development roadmap from v0.1 to v2.0
* `CHANGELOG.md <../../CHANGELOG.md>`_ - Detailed history of implemented features

See :doc:`internals/index` for detailed implementation documentation of current features.
