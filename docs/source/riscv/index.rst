RISC-V Reference Guide
======================

This section provides practical reference material for RISC-V architecture, focused on the features used in ThunderOS.

.. note::
   This is an **unofficial** reference guide created for educational purposes. 
   For authoritative information, consult the official RISC-V specifications at https://riscv.org/specifications/

.. toctree::
   :maxdepth: 2
   :caption: Topics:

   instruction_set
   privilege_levels
   csr_registers
   memory_model
   interrupts_exceptions
   assembly_guide

Overview
--------

RISC-V is an open standard instruction set architecture (ISA) based on established RISC principles. Key characteristics:

* **Open Standard**: Free and open ISA, no licensing fees
* **Modular**: Base ISA + optional extensions
* **Scalable**: From embedded to HPC systems
* **Clean Design**: Small base ISA, regular instruction encoding
* **Privilege Levels**: U (User), S (Supervisor), M (Machine)

Base ISA
~~~~~~~~

RISC-V defines several base integer ISAs:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - ISA
     - Description
   * - RV32I
     - 32-bit base integer instruction set
   * - RV64I
     - 64-bit base integer instruction set (used by ThunderOS)
   * - RV128I
     - 128-bit base integer instruction set

Standard Extensions
~~~~~~~~~~~~~~~~~~~

Common extensions (often bundled as "G"):

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Letter
     - Extension
     - Description
   * - M
     - Integer Multiply/Divide
     - mul, div, rem instructions
   * - A
     - Atomic
     - Atomic memory operations, load-reserved/store-conditional
   * - F
     - Single-Precision Float
     - 32-bit floating point
   * - D
     - Double-Precision Float
     - 64-bit floating point
   * - C
     - Compressed
     - 16-bit compressed instructions (code density)
   * - V
     - Vector
     - Vector/SIMD operations (AI/ML acceleration)

ThunderOS uses **RV64GC** (64-bit with G=IMAFD + Compressed).

Register Set
~~~~~~~~~~~~

RISC-V has 32 general-purpose integer registers (x0-x31):

.. code-block:: text

   x0:  zero (hardwired to 0)
   x1:  ra   (return address)
   x2:  sp   (stack pointer)
   x3:  gp   (global pointer)
   x4:  tp   (thread pointer)
   x5-7:    t0-t2   (temporaries)
   x8:  s0/fp (saved register / frame pointer)
   x9:  s1   (saved register)
   x10-11:  a0-a1  (function args / return values)
   x12-17:  a2-a7  (function arguments)
   x18-27:  s2-s11 (saved registers)
   x28-31:  t3-t6  (temporaries)

Plus floating-point registers (f0-f31) if F/D extensions present.

Privilege Levels
~~~~~~~~~~~~~~~~

RISC-V defines three privilege levels:

.. code-block:: text

   Level 0 (U-mode): User mode - Application code
   Level 1 (S-mode): Supervisor mode - OS kernel (ThunderOS runs here)
   Level 3 (M-mode): Machine mode - Firmware (OpenSBI)

Level 2 (H-mode for Hypervisor) is optional.

Memory Model
~~~~~~~~~~~~

RISC-V uses a **weakly-ordered memory model** (RVWMO):

* Loads and stores can be reordered by hardware
* Must use **fence** instructions for ordering guarantees
* Atomic operations (A extension) provide synchronization

Instruction Encoding
~~~~~~~~~~~~~~~~~~~~

RISC-V instructions are encoded in fixed-width formats:

* **Base instructions**: 32 bits
* **Compressed instructions** (C extension): 16 bits
* All instructions must be aligned (2-byte for C, 4-byte for base)

Why RISC-V for OS Development?
-------------------------------

Educational Benefits
~~~~~~~~~~~~~~~~~~~~

1. **Simplicity**: Clean, orthogonal instruction set
2. **Documentation**: Excellent specifications, freely available
3. **Tooling**: Mature GCC/LLVM support, QEMU emulation
4. **Transparency**: No hidden complexity or proprietary features

Technical Advantages
~~~~~~~~~~~~~~~~~~~~

1. **Modularity**: Start with base ISA, add extensions as needed
2. **Extensibility**: Custom instructions for specialized workloads
3. **Modern Design**: Avoids legacy x86/ARM baggage
4. **Open**: Can implement in hardware without licensing

For AI Workloads
~~~~~~~~~~~~~~~~

1. **Vector Extension (RVV)**: Flexible, scalable SIMD
2. **Custom Instructions**: Can add AI accelerator instructions
3. **Memory Model**: Designed for weak ordering (performance)
4. **Growing Ecosystem**: Increasing AI accelerator support

Resources
---------

Official Specifications
~~~~~~~~~~~~~~~~~~~~~~~

* **RISC-V ISA Manual**: https://riscv.org/specifications/
* **Privileged Architecture**: https://riscv.org/specifications/privileged-isa/
* **Vector Extension**: https://github.com/riscv/riscv-v-spec
* **Assembly Programmer's Manual**: https://github.com/riscv/riscv-asm-manual

Tools
~~~~~

* **QEMU**: RISC-V system emulation
* **GCC**: riscv64-unknown-elf-gcc cross-compiler
* **Spike**: RISC-V ISA simulator
* **OpenSBI**: M-mode firmware implementation

Communities
~~~~~~~~~~~

* **RISC-V International**: Main organization
* **RISC-V Software**: https://github.com/riscv
* **RISC-V Exchange**: Forums and discussions

Learning Path
-------------

For OS Development
~~~~~~~~~~~~~~~~~~

1. Start with :doc:`instruction_set` basics
2. Understand :doc:`privilege_levels` and their role
3. Learn :doc:`csr_registers` for system control
4. Study :doc:`interrupts_exceptions` mechanism
5. Practice with :doc:`assembly_guide`

Recommended Order for ThunderOS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. **Assembly Basics** - Understand bootloader code
2. **CSR Registers** - Control and status registers
3. **Privilege Levels** - S-mode vs M-mode
4. **Interrupts/Exceptions** - Trap handling
5. **Memory Model** - Paging and memory management

Next Steps
----------

Explore the following sections for detailed RISC-V reference material:

* :doc:`instruction_set` - Complete instruction reference
* :doc:`privilege_levels` - U/S/M modes explained
* :doc:`csr_registers` - Control and Status Registers
* :doc:`memory_model` - Memory ordering and paging
* :doc:`interrupts_exceptions` - Trap handling details
* :doc:`assembly_guide` - Practical assembly programming

See Also
--------

* :doc:`../internals/index` - ThunderOS implementation details
* :doc:`../internals/registers` - ThunderOS-specific register usage
* :doc:`../internals/trap_handler` - How ThunderOS handles traps
