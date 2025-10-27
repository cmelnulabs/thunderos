Instruction Set Reference
=========================

This page provides a practical reference for RISC-V instructions commonly used in OS development.

.. note::
   This focuses on **RV64I** (64-bit base) and **RV64G** extensions used by ThunderOS.

Instruction Formats
-------------------

RISC-V uses six basic instruction formats:

.. code-block:: text

   R-type: Register-register operations
   ┌──────┬──────┬──────┬──────┬──────┬──────┐
   │funct7│ rs2  │ rs1  │funct3│  rd  │opcode│
   └──────┴──────┴──────┴──────┴──────┴──────┘
    31-25  24-20  19-15  14-12  11-7   6-0
   
   I-type: Immediate operations, loads
   ┌────────────┬──────┬──────┬──────┬──────┐
   │    imm     │ rs1  │funct3│  rd  │opcode│
   └────────────┴──────┴──────┴──────┴──────┘
    31-20       19-15  14-12  11-7   6-0
   
   S-type: Stores
   ┌──────┬──────┬──────┬──────┬──────┬──────┐
   │imm[11│ rs2  │ rs1  │funct3│imm[4:│opcode│
   │  :5] │      │      │      │  0]  │      │
   └──────┴──────┴──────┴──────┴──────┴──────┘
    31-25  24-20  19-15  14-12  11-7   6-0

Base Integer (RV64I)
--------------------

Arithmetic
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - add
     - ``add rd, rs1, rs2``
     - rd = rs1 + rs2
   * - addi
     - ``addi rd, rs1, imm``
     - rd = rs1 + sign_extend(imm)
   * - sub
     - ``sub rd, rs1, rs2``
     - rd = rs1 - rs2
   * - addw
     - ``addw rd, rs1, rs2``
     - rd = sign_extend((rs1 + rs2)[31:0])
   * - addiw
     - ``addiw rd, rs1, imm``
     - rd = sign_extend((rs1 + imm)[31:0])
   * - subw
     - ``subw rd, rs1, rs2``
     - rd = sign_extend((rs1 - rs2)[31:0])

**Note**: The 'w' suffix variants operate on 32-bit values in RV64.

Logical
~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - and
     - ``and rd, rs1, rs2``
     - rd = rs1 & rs2
   * - andi
     - ``andi rd, rs1, imm``
     - rd = rs1 & sign_extend(imm)
   * - or
     - ``or rd, rs1, rs2``
     - rd = rs1 | rs2
   * - ori
     - ``ori rd, rs1, imm``
     - rd = rs1 | sign_extend(imm)
   * - xor
     - ``xor rd, rs1, rs2``
     - rd = rs1 ^ rs2
   * - xori
     - ``xori rd, rs1, imm``
     - rd = rs1 ^ sign_extend(imm)

Shift
~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - sll
     - ``sll rd, rs1, rs2``
     - Shift left logical (rs2[5:0] bits)
   * - slli
     - ``slli rd, rs1, shamt``
     - Shift left logical immediate
   * - srl
     - ``srl rd, rs1, rs2``
     - Shift right logical
   * - srli
     - ``srli rd, rs1, shamt``
     - Shift right logical immediate
   * - sra
     - ``sra rd, rs1, rs2``
     - Shift right arithmetic (sign extend)
   * - srai
     - ``srai rd, rs1, shamt``
     - Shift right arithmetic immediate

Compare
~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - slt
     - ``slt rd, rs1, rs2``
     - Set if less than (signed)
   * - slti
     - ``slti rd, rs1, imm``
     - Set if less than immediate (signed)
   * - sltu
     - ``sltu rd, rs1, rs2``
     - Set if less than (unsigned)
   * - sltiu
     - ``sltiu rd, rs1, imm``
     - Set if less than immediate (unsigned)

Load/Store
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - ld
     - ``ld rd, offset(rs1)``
     - Load doubleword (64-bit)
   * - lw
     - ``lw rd, offset(rs1)``
     - Load word (32-bit, sign-extend)
   * - lwu
     - ``lwu rd, offset(rs1)``
     - Load word unsigned (zero-extend)
   * - lh
     - ``lh rd, offset(rs1)``
     - Load halfword (16-bit, sign-extend)
   * - lhu
     - ``lhu rd, offset(rs1)``
     - Load halfword unsigned
   * - lb
     - ``lb rd, offset(rs1)``
     - Load byte (8-bit, sign-extend)
   * - lbu
     - ``lbu rd, offset(rs1)``
     - Load byte unsigned
   * - sd
     - ``sd rs2, offset(rs1)``
     - Store doubleword
   * - sw
     - ``sw rs2, offset(rs1)``
     - Store word
   * - sh
     - ``sh rs2, offset(rs1)``
     - Store halfword
   * - sb
     - ``sb rs2, offset(rs1)``
     - Store byte

Branches
~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - beq
     - ``beq rs1, rs2, offset``
     - Branch if rs1 == rs2
   * - bne
     - ``bne rs1, rs2, offset``
     - Branch if rs1 != rs2
   * - blt
     - ``blt rs1, rs2, offset``
     - Branch if rs1 < rs2 (signed)
   * - bge
     - ``bge rs1, rs2, offset``
     - Branch if rs1 >= rs2 (signed)
   * - bltu
     - ``bltu rs1, rs2, offset``
     - Branch if rs1 < rs2 (unsigned)
   * - bgeu
     - ``bgeu rs1, rs2, offset``
     - Branch if rs1 >= rs2 (unsigned)

Jumps
~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - jal
     - ``jal rd, offset``
     - Jump and link (rd = pc+4, pc += offset)
   * - jalr
     - ``jalr rd, offset(rs1)``
     - Jump and link register (rd = pc+4, pc = rs1+offset)

Upper Immediate
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - lui
     - ``lui rd, imm``
     - Load upper immediate (rd = imm << 12)
   * - auipc
     - ``auipc rd, imm``
     - Add upper immediate to PC (rd = pc + (imm << 12))

System
~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - ecall
     - ``ecall``
     - Environment call (trap to higher privilege)
   * - ebreak
     - ``ebreak``
     - Breakpoint (trap for debugger)
   * - fence
     - ``fence pred, succ``
     - Memory ordering fence
   * - fence.i
     - ``fence.i``
     - Instruction fence (synchronize I-cache)

CSR Instructions
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - Instruction
     - Syntax
     - Description
   * - csrrw
     - ``csrrw rd, csr, rs1``
     - Read/write CSR (rd = csr, csr = rs1)
   * - csrrs
     - ``csrrs rd, csr, rs1``
     - Read and set bits (rd = csr, csr |= rs1)
   * - csrrc
     - ``csrrc rd, csr, rs1``
     - Read and clear bits (rd = csr, csr &= ~rs1)
   * - csrrwi
     - ``csrrwi rd, csr, imm``
     - Read/write CSR immediate
   * - csrrsi
     - ``csrrsi rd, csr, imm``
     - Read and set bits immediate
   * - csrrci
     - ``csrrci rd, csr, imm``
     - Read and clear bits immediate

Multiply/Divide (M Extension)
------------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Instruction
     - Syntax
     - Description
   * - mul
     - ``mul rd, rs1, rs2``
     - Multiply (lower 64 bits)
   * - mulh
     - ``mulh rd, rs1, rs2``
     - Multiply high (signed × signed, upper 64 bits)
   * - mulhu
     - ``mulhu rd, rs1, rs2``
     - Multiply high unsigned
   * - mulhsu
     - ``mulhsu rd, rs1, rs2``
     - Multiply high signed-unsigned
   * - div
     - ``div rd, rs1, rs2``
     - Divide (signed)
   * - divu
     - ``divu rd, rs1, rs2``
     - Divide unsigned
   * - rem
     - ``rem rd, rs1, rs2``
     - Remainder (signed)
   * - remu
     - ``remu rd, rs1, rs2``
     - Remainder unsigned

Atomic (A Extension)
--------------------

Load-Reserved / Store-Conditional
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - Instruction
     - Syntax
     - Description
   * - lr.d
     - ``lr.d rd, (rs1)``
     - Load-reserved doubleword
   * - sc.d
     - ``sc.d rd, rs2, (rs1)``
     - Store-conditional (rd=0 success, rd=1 fail)

Atomic Memory Operations
~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 30 55

   * - Instruction
     - Syntax
     - Description
   * - amoswap.d
     - ``amoswap.d rd, rs2, (rs1)``
     - Atomic swap
   * - amoadd.d
     - ``amoadd.d rd, rs2, (rs1)``
     - Atomic add
   * - amoand.d
     - ``amoand.d rd, rs2, (rs1)``
     - Atomic AND
   * - amoor.d
     - ``amoor.d rd, rs2, (rs1)``
     - Atomic OR
   * - amoxor.d
     - ``amoxor.d rd, rs2, (rs1)``
     - Atomic XOR
   * - amomax.d
     - ``amomax.d rd, rs2, (rs1)``
     - Atomic maximum (signed)
   * - amomaxu.d
     - ``amomaxu.d rd, rs2, (rs1)``
     - Atomic maximum (unsigned)
   * - amomin.d
     - ``amomin.d rd, rs2, (rs1)``
     - Atomic minimum (signed)
   * - amominu.d
     - ``amominu.d rd, rs2, (rs1)``
     - Atomic minimum (unsigned)

**Note**: All atomic operations can have ordering modifiers (aq/rl).

Pseudo-Instructions
-------------------

The assembler provides convenient pseudo-instructions:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Pseudo
     - Expands To
     - Description
   * - ``nop``
     - ``addi x0, x0, 0``
     - No operation
   * - ``li rd, imm``
     - ``lui`` + ``addi``
     - Load immediate (any value)
   * - ``mv rd, rs``
     - ``addi rd, rs, 0``
     - Move register
   * - ``not rd, rs``
     - ``xori rd, rs, -1``
     - Bitwise NOT
   * - ``neg rd, rs``
     - ``sub rd, x0, rs``
     - Negate
   * - ``la rd, symbol``
     - ``auipc`` + ``addi``
     - Load address
   * - ``call offset``
     - ``auipc`` + ``jalr``
     - Function call
   * - ``ret``
     - ``jalr x0, 0(x1)``
     - Return from function
   * - ``j offset``
     - ``jal x0, offset``
     - Unconditional jump
   * - ``jr rs``
     - ``jalr x0, 0(rs)``
     - Jump register
   * - ``beqz rs, offset``
     - ``beq rs, x0, offset``
     - Branch if equal to zero
   * - ``bnez rs, offset``
     - ``bne rs, x0, offset``
     - Branch if not equal to zero
   * - ``csrr rd, csr``
     - ``csrrs rd, csr, x0``
     - Read CSR
   * - ``csrw csr, rs``
     - ``csrrw x0, csr, rs``
     - Write CSR
   * - ``csrs csr, rs``
     - ``csrrs x0, csr, rs``
     - Set bits in CSR
   * - ``csrc csr, rs``
     - ``csrrc x0, csr, rs``
     - Clear bits in CSR
   * - ``csrwi csr, imm``
     - ``csrrwi x0, csr, imm``
     - Write CSR immediate
   * - ``csrsi csr, imm``
     - ``csrrsi x0, csr, imm``
     - Set bits immediate
   * - ``csrci csr, imm``
     - ``csrrci x0, csr, imm``
     - Clear bits immediate

Privilege Instructions
----------------------

These instructions change privilege level:

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Instruction
     - Description
   * - ``sret``
     - Return from supervisor mode (S-mode trap handler)
   * - ``mret``
     - Return from machine mode (M-mode trap handler)
   * - ``wfi``
     - Wait for interrupt (hint to reduce power)
   * - ``sfence.vma``
     - Supervisor fence for virtual memory (flush TLB)

Common Patterns
---------------

Loading Constants
~~~~~~~~~~~~~~~~~

.. code-block:: asm

   # Load small immediate (-2048 to 2047)
   addi t0, zero, 42
   
   # Load 32-bit immediate
   lui t0, %hi(0x12345678)
   addi t0, t0, %lo(0x12345678)
   
   # Pseudo-instruction (assembler does above)
   li t0, 0x12345678

Loading Addresses
~~~~~~~~~~~~~~~~~

.. code-block:: asm

   # Load address of symbol
   auipc t0, %pcrel_hi(my_data)
   addi t0, t0, %pcrel_lo(my_data)
   
   # Pseudo-instruction
   la t0, my_data

Function Calls
~~~~~~~~~~~~~~

.. code-block:: asm

   # Call function
   call my_function
   
   # Expands to:
   auipc ra, %pcrel_hi(my_function)
   jalr ra, %pcrel_lo(my_function)(ra)
   
   # Return
   ret  # jalr x0, 0(ra)

Conditional Branches
~~~~~~~~~~~~~~~~~~~~

.. code-block:: asm

   # if (a0 == a1) goto equal
   beq a0, a1, equal
   
   # if (a0 != 0) goto nonzero
   bnez a0, nonzero
   
   # if (a0 < a1) goto less (signed)
   blt a0, a1, less
   
   # if (a0 >= a1) goto greater_equal (unsigned)
   bgeu a0, a1, greater_equal

Memory Access
~~~~~~~~~~~~~

.. code-block:: asm

   # Load word from memory
   lw t0, 0(sp)        # t0 = *(sp + 0)
   lw t1, 8(sp)        # t1 = *(sp + 8)
   
   # Store doubleword
   sd a0, 16(sp)       # *(sp + 16) = a0
   
   # Load address then dereference
   la t0, my_var
   ld t1, 0(t0)

CSR Operations
~~~~~~~~~~~~~~

.. code-block:: asm

   # Read CSR
   csrr t0, sstatus
   
   # Write CSR
   csrw sstatus, t0
   
   # Set bits in CSR
   csrsi sie, 0x20     # Set bit 5 (timer interrupt)
   
   # Clear bits in CSR
   csrci sstatus, 0x2  # Clear bit 1 (disable interrupts)
   
   # Atomic read-modify-write
   csrrs t0, sie, t1   # t0 = sie, sie |= t1

Stack Operations
~~~~~~~~~~~~~~~~

.. code-block:: asm

   # Push registers to stack
   addi sp, sp, -16
   sd ra, 0(sp)
   sd s0, 8(sp)
   
   # Pop registers from stack
   ld ra, 0(sp)
   ld s0, 8(sp)
   addi sp, sp, 16

Instruction Timing
------------------

Typical Latencies (Implementation-Dependent)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Operation
     - Typical Latency
   * - Integer ALU
     - 1 cycle
   * - Load
     - 2-3 cycles (cache hit)
   * - Store
     - 1 cycle (to store buffer)
   * - Branch (taken)
     - 2-3 cycles (if predicted)
   * - Multiply
     - 3-5 cycles
   * - Divide
     - 10-40 cycles
   * - CSR access
     - 1-2 cycles
   * - Atomic operation
     - 10+ cycles

**Note**: QEMU emulation doesn't accurately model timing.

See Also
--------

* :doc:`csr_registers` - Control and Status Register details
* :doc:`assembly_guide` - Practical assembly programming
* :doc:`privilege_levels` - Instruction access by privilege level
* Official RISC-V ISA Manual: https://riscv.org/specifications/
