Interrupts and Exceptions
=========================

RISC-V distinguishes between synchronous exceptions and asynchronous interrupts, collectively called "traps".

Trap Types
----------

**Exceptions** (synchronous):

* Illegal instruction
* Page faults
* Ecall (system call)
* Breakpoint

**Interrupts** (asynchronous):

* Timer interrupt
* Software interrupt  
* External interrupt

Trap Handling
--------------

See :doc:`../internals/trap_handler` for complete ThunderOS trap handling implementation.

Trap Flow
~~~~~~~~~

1. Trap occurs → Hardware saves PC to ``sepc``
2. Hardware sets ``scause`` to trap cause
3. Hardware disables interrupts (clear ``sstatus.SIE``)
4. Hardware jumps to ``stvec``
5. Trap handler executes
6. Handler uses ``sret`` to return

See Also
--------

* :doc:`../internals/trap_handler` - ThunderOS trap implementation
* :doc:`../internals/timer_clint` - Timer interrupt details
* :doc:`privilege_levels` - Trap delegation
