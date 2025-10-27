Timer and CLINT
===============

ThunderOS uses the CLINT timer to generate periodic interrupts for timekeeping and future scheduling.

For RISC-V timer architecture and SBI details, see :doc:`../riscv/privilege_levels`.

Overview
--------

ThunderOS runs in S-mode and programs the timer via SBI calls to OpenSBI firmware (M-mode). Timer interrupts are handled through the trap handler.

Architecture
------------

Components
~~~~~~~~~~

.. code-block:: text

   kernel/drivers/clint.c  - CLINT timer driver
   include/clint.h         - Timer interface and constants

Timer Flow
~~~~~~~~~~

.. code-block:: text

   [Kernel Init]
        |
        v
   clint_init()
        |
        ├─> Enable timer interrupts in sie register
        ├─> Enable global interrupts in sstatus
        ├─> Set first timer interrupt via SBI
        |
        v
   [Kernel Idle Loop - WFI]
        |
        v
   [Timer Fires] ──> Hardware Interrupt
        |
        v
   [Trap Handler]
        |
        v
   handle_interrupt() ──> Identifies timer interrupt (cause = 5)
        |
        v
   clint_handle_timer()
        |
        ├─> Increment tick counter
        ├─> Schedule next interrupt via SBI
        └─> Return from interrupt
        |
        v
   [Resume Kernel Execution]

SBI Timer Interface
-------------------

ThunderOS programs the timer via SBI calls to OpenSBI (M-mode).

For SBI mechanism details, see :doc:`../riscv/privilege_levels`.

Reading Time
------------

ThunderOS uses the ``rdtime`` instruction to read the current time counter.

**QEMU frequency**: 10 MHz (100ns per tick, 10,000 ticks/ms)

CLINT Driver Implementation
----------------------------

Locations:

* ``kernel/drivers/clint.c`` - Driver implementation
* ``include/clint.h`` - Interface and constants

Initialization
--------------

Called from ``kernel_main()`` to enable timer interrupts and schedule first interrupt (1 second interval).

For CSR details, see :doc:`../riscv/csr_registers`.

Interrupt Handling
------------------

See :doc:`trap_handler` for trap mechanism.

Timer interrupt flow:

1. Hardware jumps to ``trap_vector``
2. ``trap_handler()`` identifies timer interrupt (scause = 5)
3. ``handle_interrupt()`` calls ``clint_handle_timer()``
4. Handler increments tick counter and schedules next interrupt

Usage Examples
--------------

See ``kernel/drivers/clint.c`` for API:

* ``clint_get_ticks()`` - Get elapsed seconds
* ``read_time()`` - Get high-resolution time
* ``clint_set_timer()`` - Schedule next interrupt

Testing
-------

See :doc:`testing_framework` for timer test suite and instructions.

Debugging
---------

See :doc:`testing_framework` for debugging and GDB usage.

Performance
-----------

**Interrupt overhead** (QEMU): ~120 cycles/interrupt (~50 save + 20 handler + 50 restore)

At 1 Hz: negligible. At 1000 Hz (preemptive scheduling): ~1% overhead.

Future Enhancements
-------------------

* Configurable frequency (runtime adjustment)
* High-resolution one-shot timers
* Tickless kernel (power saving)
* Preemptive multitasking with time slicing
* Wall clock time (RTC integration)

See Also
--------

* :doc:`trap_handler` - Interrupt handling
* :doc:`../riscv/privilege_levels` - SBI and timer architecture
* :doc:`../riscv/csr_registers` - Timer CSR details
* :doc:`testing_framework` - Test infrastructure

