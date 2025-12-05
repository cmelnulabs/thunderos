Linker Script
=============

The linker script (``kernel/arch/riscv64/kernel.ld``) defines how the kernel
binary is laid out in memory. It controls where code, data, and other sections
are placed.

Overview
--------

**File:** ``kernel/arch/riscv64/kernel.ld``

**Purpose:**
   * Define memory layout
   * Place sections at specific addresses
   * Provide symbols for bootloader and kernel
   * Discard unnecessary sections

**Linker:** GNU ld (``riscv64-unknown-elf-ld``)

Source Code
-----------

.. code-block:: ld

   /* kernel/arch/riscv64/kernel.ld */
   
   OUTPUT_ARCH(riscv)
   ENTRY(_start)

   SECTIONS
   {
       . = 0x80200000;
       
       .text : {
           PROVIDE(_text_start = .);
           *(.text.boot)
           *(.text .text.*)
           PROVIDE(_text_end = .);
       }
       
       .rodata : {
           PROVIDE(_rodata_start = .);
           *(.rodata .rodata.*)
           PROVIDE(_rodata_end = .);
       }
       
       .data : {
           PROVIDE(_data_start = .);
           *(.data .data.*)
           PROVIDE(_data_end = .);
       }
       
       .bss : {
           PROVIDE(_bss_start = .);
           *(.bss .bss.*)
           *(COMMON)
           PROVIDE(_bss_end = .);
       }
       
       . = ALIGN(4096);
       PROVIDE(_kernel_end = .);
       
       /DISCARD/ : {
           *(.comment)
           *(.eh_frame)
       }
   }

Detailed Explanation
--------------------

Program Headers (Memory Protection)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   PHDRS
   {
       text PT_LOAD FLAGS(5);  /* R + X = 5 */
       data PT_LOAD FLAGS(6);  /* R + W = 6 */
   }

**Purpose:**
   * Create separate ELF program headers for different memory regions
   * Enable W^X (Write XOR Execute) memory protection
   * Prevent code execution from writable memory

**Program Header Types:**

* **text segment**: Read + Execute (FLAGS = 5)
  
  - Contains ``.text`` (code) and ``.rodata`` (constants)
  - Cannot be written to at runtime
  - Can be executed

* **data segment**: Read + Write (FLAGS = 6)
  
  - Contains ``.data`` (initialized data) and ``.bss`` (zero-initialized)
  - Can be written to at runtime
  - Cannot be executed

**Flag Values:**

.. code-block:: text

   PF_X (Execute) = 1
   PF_W (Write)   = 2
   PF_R (Read)    = 4
   
   R + X = 4 + 1 = 5
   R + W = 4 + 2 = 6

**Security Benefits:**

- Prevents code injection attacks (can't write to executable memory)
- Prevents data execution attacks (can't execute writable memory)
- Follows principle of least privilege
- Standard practice for modern operating systems

**Verification:**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -l build/thunderos.elf
   
   Program Headers:
     Type      Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
     LOAD      0x001000 0x0000000080000000 0x0000000080000000 0x019295 0x019295 R E 0x1000
     LOAD      0x01b000 0x000000008001a000 0x000000008001a000 0x0000d4 0x012830 RW  0x1000

First segment (R E): Code and read-only data
Second segment (RW): Read-write data

Header Directives
~~~~~~~~~~~~~~~~~

.. code-block:: ld

   OUTPUT_ARCH(riscv)
   ENTRY(_start)

**OUTPUT_ARCH(riscv)**
   * Specifies target architecture
   * Tells linker to generate RISC-V binary
   * Must match compiler target (riscv64)

**ENTRY(_start)**
   * Defines entry point symbol
   * QEMU/debugger use this to know where execution begins
   * Must match global symbol in ``boot.S``

Base Address
~~~~~~~~~~~~

.. code-block:: ld

   . = 0x80200000;

**Location Counter (`.`)**
   * Special variable representing current address
   * Assignment sets starting address for all sections

**Why 0x80000000?**
   * With ``-bios none``, QEMU starts execution directly at 0x80000000
   * No external firmware (OpenSBI) - ThunderOS handles M-mode initialization
   * Full control of the boot process from the first instruction

**Memory Map (QEMU virt machine):**

.. code-block:: text

   ┌─────────────────────┬──────────────┬─────────────────────────────────────┐
   │ Address Range       │ Size         │ Description                         │
   ├─────────────────────┼──────────────┼─────────────────────────────────────┤
   │ 0x00001000          │ 4KB          │ Boot ROM (QEMU reset vector)        │
   │ 0x02000000          │ 64KB         │ CLINT (Core Local Interruptor)      │
   │ 0x0C000000          │ 64MB         │ PLIC (Platform-Level Interrupt Ctr) │
   │ 0x10000000          │ 256B         │ UART0 (NS16550A serial console)     │
   │ 0x10001000+         │ varies       │ VirtIO MMIO devices (block, GPU)    │
   ├─────────────────────┼──────────────┼─────────────────────────────────────┤
   │ 0x80000000          │ ~1MB         │ ThunderOS Kernel                    │
   │   ├─ .text.entry    │              │   M-mode entry point (entry.S)      │
   │   ├─ .text.boot     │              │   S-mode boot code (boot.S)         │
   │   ├─ .text          │              │   Kernel code                       │
   │   ├─ .rodata        │              │   Read-only data                    │
   │   ├─ .data          │              │   Initialized data                  │
   │   └─ .bss           │              │   Uninitialized data                │
   │ _kernel_end         │              │   End of kernel image               │
   ├─────────────────────┼──────────────┼─────────────────────────────────────┤
   │ _kernel_end -       │ ~127MB       │ Free RAM (PMM, DMA, user processes) │
   │ 0x88000000          │              │                                     │
   └─────────────────────┴──────────────┴─────────────────────────────────────┘

   Total RAM: 128MB (0x80000000 - 0x88000000)

Text Section
~~~~~~~~~~~~

.. code-block:: ld

   .text : {
       PROVIDE(_text_start = .);
       *(.text.entry)     /* M-mode entry */
       *(.text.boot)      /* S-mode boot */
       *(.text .text.*)
       PROVIDE(_text_end = .);
   } :text

**Purpose:** Executable code

**Components:**

* ``*(.text.entry)`` - M-mode entry point from entry.S
* ``*(.text.boot)`` - S-mode bootloader code from boot.S
* ``*(.text .text.*)`` - All other code sections
* ``PROVIDE(_text_start/_text_end)`` - Symbol markers for debugging
* ``:text`` - Assign to text program header (R+X permissions)

**Ordering Matters:**
   * ``.text.entry`` MUST come first (M-mode entry)
   * ``.text.boot`` follows (S-mode initialization)
   * Contains ``_entry`` entry point
   * First instruction executed after QEMU starts

**Example Contents:**

.. code-block:: text

   _text_start:
   0x80000000:  _entry (entry.S, M-mode)
   0x80000100:  _start (boot.S, S-mode)
   0x80000200:  kernel_main (main.c)
   0x80000300:  uart_init (uart.c)
   ...
   _text_end:

Read-Only Data Section
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .rodata : {
       PROVIDE(_rodata_start = .);
       *(.rodata .rodata.*)
       PROVIDE(_rodata_end = .);
   } :text

**Purpose:** Constant data (strings, const variables)

**Assignment:** ``:text`` - Same segment as code (R+X, no write permission)

**Example Contents:**

.. code-block:: c

   // In C code:
   const char *msg = "ThunderOS";  // Pointer in .data, string in .rodata
   const int MAX = 100;            // Value in .rodata

**Memory Protection:**
   * Part of read-only, executable segment
   * Hardware enforced (no writes allowed)
   * Attempts to write trigger exception

Page Alignment for Data
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   . = ALIGN(4096);

**Purpose:** Align to page boundary before data sections

**Why Needed:**
   * Data segment has different permissions (R+W vs R+X)
   * MMU/page tables work on 4KB page granularity
   * Separate LOAD segments must start on page boundaries

**Effect:**
   * Pads between .rodata and .data if needed
   * Ensures clean separation for memory protection
   * May waste some space but necessary for security

Data Section
~~~~~~~~~~~~

.. code-block:: ld

   .data : {
       PROVIDE(_data_start = .);
       *(.data .data.*)
       PROVIDE(_data_end = .);
   } :data

**Purpose:** Initialized global/static variables

**Assignment:** ``:data`` - Assign to data program header (R+W permissions)

**Example Contents:**

.. code-block:: c

   // In C code:
   int counter = 42;              // In .data
   static char buffer[256] = {};  // In .data (explicitly initialized)

**vs. .rodata:**
   * ``.data`` - read/write
   * ``.rodata`` - read-only

BSS Section
~~~~~~~~~~~

.. code-block:: ld

   .bss : {
       PROVIDE(_bss_start = .);
       *(.bss .bss.*)
       *(COMMON)
       PROVIDE(_bss_end = .);
   } :data

**Purpose:** Zero-initialized variables

**Assignment:** ``:data`` - Same segment as .data (R+W permissions)

**BSS = Block Started by Symbol (historical)**

**Example Contents:**

.. code-block:: c

   // In C code:
   int uninitialized;             // In .bss
   static char buffer[1024];      // In .bss (no initializer)

**Special Property:**
   * Not stored in binary file
   * Only size recorded
   * Bootloader zeros this region
   * Saves disk space

**COMMON:**
   * Tentative definitions (C quirk)
   * ``int x;`` in multiple files
   * Linker merges into single variable

Kernel End Marker
~~~~~~~~~~~~~~~~~

.. code-block:: ld

   . = ALIGN(4096);
   PROVIDE(_kernel_end = .);

**ALIGN(4096):**
   * Rounds up to next 4KB boundary
   * 4096 = page size in RISC-V
   * Ensures kernel ends at page boundary

**_kernel_end:**
   * Symbol marking end of kernel
   * Useful for memory allocator
   * Free memory starts here

**Example:**

.. code-block:: text

   .bss ends at:     0x80250ABC
   After alignment:  0x80251000  ← _kernel_end
   Free memory:      0x80251000 - 0x87FFFFFF

Discarded Sections
~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   /DISCARD/ : {
       *(.comment)
       *(.eh_frame)
   }

**Purpose:** Remove unnecessary sections

**.comment:**
   * Compiler version strings
   * Build timestamps
   * Not needed in final kernel

**.eh_frame:**
   * Exception handling metadata (C++)
   * Used for stack unwinding
   * ThunderOS doesn't use C++ or exceptions

**Why discard?**
   * Reduces binary size
   * Faster loading
   * Cleaner memory layout

Symbols Provided
----------------

The linker script provides these symbols to C/assembly code:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Symbol
     - Description
   * - ``_text_start``
     - Start of code section
   * - ``_text_end``
     - End of code section
   * - ``_rodata_start``
     - Start of read-only data
   * - ``_rodata_end``
     - End of read-only data
   * - ``_data_start``
     - Start of initialized data
   * - ``_data_end``
     - End of initialized data
   * - ``_bss_start``
     - Start of uninitialized data
   * - ``_bss_end``
     - End of uninitialized data
   * - ``_kernel_end``
     - End of entire kernel

Using Symbols in C
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Declare as extern
   extern char _bss_start[];
   extern char _bss_end[];
   extern char _kernel_end[];

   void show_memory_layout(void) {
       uart_printf("BSS: %p - %p\n", _bss_start, _bss_end);
       uart_printf("Kernel ends at: %p\n", _kernel_end);
       
       size_t bss_size = _bss_end - _bss_start;
       uart_printf("BSS size: %zu bytes\n", bss_size);
   }

**Note:** Symbols are addresses, not variables!

.. code-block:: c

   // WRONG:
   int size = _bss_end - _bss_start;  // Subtracts garbage values
   
   // CORRECT:
   int size = (char*)&_bss_end - (char*)&_bss_start;
   // or:
   extern char _bss_start[], _bss_end[];
   int size = _bss_end - _bss_start;  // Works if declared as arrays

Section Attributes in C
-----------------------

You can control section placement from C code:

Placing Code in Specific Section
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Put function in .text.boot section
   void __attribute__((section(".text.boot"))) early_init(void) {
       // This runs before main kernel
   }

Placing Data in Specific Section
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Put variable in .data section (even if uninitialized)
   int __attribute__((section(".data"))) important_var;
   
   // Custom section
   const char __attribute__((section(".version"))) version[] = "v1.0";

Memory Layout Example
---------------------

After linking, the memory looks like:

.. code-block:: text

   Address       Section       Contents
   ────────────────────────────────────────────────────
   0x80200000    .text.boot    _start: csrw sie, zero
                               la sp, _stack_top
                               ...
   
   0x80200100    .text         kernel_main: uart_init()
                               uart_puts("...")
                               ...
   
   0x80201000    .rodata       "ThunderOS\0"
                               "Kernel loaded\0"
                               ...
   
   0x80202000    .data         (none currently)
   
   0x80203000    .bss          _stack_bottom:
                               .space 16384
                               _stack_top:
   
   0x80207000    (end)         _kernel_end (page aligned)
   
   0x80208000    (free)        Available RAM

Binary vs. ELF
--------------

The build produces two files:

**thunderos.elf**
   * Executable and Linkable Format
   * Contains sections, symbols, debug info
   * Used by debugger (GDB)
   * QEMU can load directly

**thunderos.bin**
   * Raw binary (just bytes)
   * No metadata
   * Smaller than ELF
   * Used for real hardware / bootloaders

Checking Layout
~~~~~~~~~~~~~~~

.. code-block:: bash

   # Show section sizes
   riscv64-unknown-elf-size build/thunderos.elf
   
   # Show section addresses
   riscv64-unknown-elf-readelf -S build/thunderos.elf
   
   # Show all symbols
   riscv64-unknown-elf-nm build/thunderos.elf
   
   # Verify entry point
   riscv64-unknown-elf-readelf -h build/thunderos.elf | grep Entry

Common Issues
-------------

Section Overlap
~~~~~~~~~~~~~~~

**Error:** "section .data overlaps section .text"

**Cause:** Sections too large, running into each other

**Solution:** Check section sizes, increase base address spacing

.. code-block:: ld

   .text : { ... }
   . = ALIGN(0x1000);  # Force gap
   .rodata : { ... }

Missing Symbols
~~~~~~~~~~~~~~~

**Error:** "undefined reference to `_bss_start`"

**Cause:** Symbol not provided by linker script

**Solution:** Add ``PROVIDE(_bss_start = .);``

Wrong Entry Point
~~~~~~~~~~~~~~~~~

**Error:** Kernel doesn't boot, or crashes immediately

**Cause:** ``ENTRY(_start)`` doesn't match actual entry function

**Solution:** Verify ``_start`` is global and in ``.text.boot``

.. code-block:: asm

   .global _start  # Must be global!
   _start:
       ...

Alignment Issues
~~~~~~~~~~~~~~~~

**Error:** Unaligned access trap

**Cause:** Data structures not aligned properly

**Solution:** Add alignment directives

.. code-block:: ld

   .data : ALIGN(8) {
       *(.data .data.*)
   }

Advanced Topics
---------------

Multiple Load Regions
~~~~~~~~~~~~~~~~~~~~~

For systems with RAM and ROM:

.. code-block:: ld

   MEMORY {
       ROM : ORIGIN = 0x00000000, LENGTH = 1M
       RAM : ORIGIN = 0x80000000, LENGTH = 128M
   }

   SECTIONS {
       .text : { ... } > ROM
       .data : { ... } > RAM AT> ROM
   }

Custom Sections
~~~~~~~~~~~~~~~

Create custom sections for special purposes:

.. code-block:: ld

   .ai_models : {
       PROVIDE(_ai_models_start = .);
       *(.ai_models)
       PROVIDE(_ai_models_end = .);
   }

Then in C:

.. code-block:: c

   const uint8_t __attribute__((section(".ai_models"))) 
       neural_net_weights[] = { ... };

Debugging Linker Scripts
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Verbose linker output
   riscv64-unknown-elf-ld -verbose ...
   
   # Generate map file
   riscv64-unknown-elf-ld -Map=kernel.map ...
   
   # Show where symbols came from
   grep symbol_name kernel.map

See Also
--------

* :doc:`bootloader` - Uses _bss_start/_bss_end symbols
* :doc:`memory` - Complete memory management and layout
* `GNU ld Manual <https://sourceware.org/binutils/docs/ld/>`_
