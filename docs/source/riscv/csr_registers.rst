CSR Registers
=============

Control and Status Registers (CSRs) provide the interface to configure and query processor state.

.. note::
   This is a quick reference. For complete details, see the RISC-V Privileged Specification.

Overview
--------

CSRs are special registers accessed via ``csrrw``, ``csrrs``, ``csrrc`` and their immediate variants.

Common S-mode CSRs
------------------

See :doc:`../internals/registers` for detailed ThunderOS usage.

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - CSR
     - Address
     - Description
   * - ``sstatus``
     - 0x100
     - Supervisor status register
   * - ``sie``
     - 0x104
     - Supervisor interrupt enable
   * - ``stvec``
     - 0x105
     - Supervisor trap vector
   * - ``sscratch``
     - 0x140
     - Supervisor scratch register
   * - ``sepc``
     - 0x141
     - Supervisor exception program counter
   * - ``scause``
     - 0x142
     - Supervisor trap cause
   * - ``stval``
     - 0x143
     - Supervisor trap value
   * - ``sip``
     - 0x144
     - Supervisor interrupt pending
   * - ``satp``
     - 0x180
     - Supervisor address translation

See Also
--------

* :doc:`../internals/registers` - Complete register reference for ThunderOS
* :doc:`privilege_levels` - Which CSRs are accessible at each level
