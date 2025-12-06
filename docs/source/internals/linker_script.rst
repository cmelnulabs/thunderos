Linker Script
=============

The linker script (``kernel/arch/riscv64/kernel.ld``) is a critical component that defines how the ThunderOS kernel binary is laid out in memory. It controls the precise placement of code, data, and other sections, ensuring proper alignment, memory protection, and boot sequencing.

Overview
--------

**File:** ``kernel/arch/riscv64/kernel.ld``

**Purpose:**
   * Define exact memory layout starting at 0x80000000 (QEMU ``-bios none`` entry point)
   * Order sections for proper boot sequence (``.text.entry`` must be first)
   * Create separate program headers for W^X (Write XOR Execute) memory protection
   * Provide symbols for bootloader (``_bss_start``, ``_bss_end``) and memory management (``_kernel_end``)
   * Align sections to page boundaries (4KB) for MMU/paging setup
   * Discard unnecessary sections to reduce binary size

**Linker:** GNU ld (``riscv64-unknown-elf-ld``)

**Key Constraints:**
   * Entry point MUST be at 0x80000000 (where QEMU jumps with ``-bios none``)
   * ``.text.entry`` section MUST be first (M-mode entry point from ``entry.S``)
   * Code and data must be in separate program headers for memory protection
   * All addresses must be in physical memory range (no virtual addressing yet)

Complete Source Code
--------------------

.. code-block:: ld

   /*
    * Linker script for ThunderOS kernel
    * RISC-V 64-bit
    * 
    * With -bios none, QEMU starts execution at 0x80000000 in M-mode
    */
   
   OUTPUT_ARCH(riscv)
   ENTRY(_entry)
   
   PHDRS
   {
       text PT_LOAD FLAGS(5);  /* R + X = 5 */
       data PT_LOAD FLAGS(6);  /* R + W = 6 */
   }
   
   SECTIONS
   {
       /* Kernel starts at 0x80000000 (QEMU -bios none entry point) */
       . = 0x80000000;
       /* Kernel starts at 0x80000000 (QEMU -bios none entry point) */
       . = 0x80000000;
       
       .text : {
           PROVIDE(_text_start = .);
           *(.text.entry)     /* M-mode entry from entry.S */
           *(.text.boot)      /* S-mode boot from boot.S */
           *(.text .text.*)
           PROVIDE(_text_end = .);
       } :text
       
       .rodata : {
           PROVIDE(_rodata_start = .);
           *(.rodata .rodata.*)
           PROVIDE(_rodata_end = .);
       } :text
       
       . = ALIGN(4096);  /* Align to page boundary for different permissions */
       
       .data : {
           PROVIDE(_data_start = .);
           *(.data .data.*)
           PROVIDE(_data_end = .);
       } :data
       
       .bss : {
           PROVIDE(_bss_start = .);
           *(.bss .bss.*)
           *(COMMON)
           PROVIDE(_bss_end = .);
       } :data
       
       . = ALIGN(4096);
       PROVIDE(_kernel_end = .);
       
       /* Embedded test programs */
       .user_exception_test : {
           user_exception_start = .;
           *(.user_exception_test)
           user_exception_end = .;
       } :data
       
       /DISCARD/ : {
           *(.comment)
           *(.eh_frame)
       }
   }

**Actual Memory Layout (from built kernel):**

.. code-block:: text

   Address        Size      Section      Description
   ─────────────────────────────────────────────────────────────────
   0x80000000     ~145KB    .text        Executable code (R+X)
                            .rodata      Read-only data (R+X)
   ─────────────────────────────────────────────────────────────────
   0x80024000     212B      .data        Initialized data (R+W)
                            .sdata       Small data (R+W)
                  188KB     .bss         Zero-initialized (R+W)
                            .sbss        Small BSS (R+W)
   ─────────────────────────────────────────────────────────────────
   0x80055000     -         _kernel_end  Free memory starts here
   
   Total kernel size: ~330KB (338,606 bytes)

Detailed Explanation
--------------------

1. Program Headers (Memory Protection)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   PHDRS
   {
       text PT_LOAD FLAGS(5);  /* R + X = 5 */
       data PT_LOAD FLAGS(6);  /* R + W = 6 */
   }

**What are Program Headers?**

Program headers (also called segments) are part of the ELF (Executable and Linkable Format) file structure. They tell the loader (QEMU, or eventually a real bootloader) how to load the binary into memory and what permissions each region should have.

**Why Do We Need Them?**

Modern operating systems follow the **W^X (Write XOR Execute)** security principle:

* Memory that is **writable** should NOT be **executable**
* Memory that is **executable** should NOT be **writable**

This prevents common exploits:

* **Code injection attacks**: Can't write malicious code to executable memory
* **Data execution attacks**: Can't trick the CPU into executing data as code

**ThunderOS Program Headers:**

.. list-table::
   :header-rows: 1
   :widths: 15 15 70

   * - Segment
     - FLAGS
     - Purpose
   * - ``text``
     - 5 (R+X)
     - Contains code and constants. Read + Execute, but NOT writable.
   * - ``data``
     - 6 (R+W)
     - Contains variables and BSS. Read + Write, but NOT executable.

**Flag Encoding:**

The FLAGS value is a bitmask of standard ELF permissions:

.. code-block:: text

   PF_X (Execute) = 1  (bit 0)
   PF_W (Write)   = 2  (bit 1)
   PF_R (Read)    = 4  (bit 2)
   
   Read + Execute = 4 + 1 = 5 = 0b101
   Read + Write   = 4 + 2 = 6 = 0b110

**How ThunderOS Uses This:**

.. code-block:: text

   .text section    } 
   .rodata section  } → text segment (R+X) → Cannot be modified at runtime
   
   .data section    }
   .bss section     } → data segment (R+W) → Cannot be executed

**Verification:**

You can verify the program headers in the built kernel:

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -l build/thunderos.elf
   
   Program Headers:
     Type      Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
     LOAD      0x001000 0x0000000080000000 0x0000000080000000 0x0239b2 0x0239b2 R E 0x1000
     LOAD      0x025000 0x0000000080024000 0x0000000080024000 0x0000d4 0x030028 RW  0x1000

* First LOAD: Code + rodata (R E = Read Execute, 145KB)
* Second LOAD: Data + BSS (RW = Read Write, 192KB)

**Important:** These permissions are currently advisory. Once ThunderOS sets up the MMU and page tables, these will be enforced by hardware, causing exceptions if violated.

2. Output Architecture and Entry Point
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   OUTPUT_ARCH(riscv)
   ENTRY(_entry)

**OUTPUT_ARCH(riscv)**
   * Specifies target architecture: RISC-V
   * Tells the linker to generate RISC-V 64-bit code
   * Must match compiler target (``riscv64-unknown-elf-gcc``)
   * Affects instruction encoding, calling conventions, and ABI

**ENTRY(_entry)**
   * Defines the entry point symbol: ``_entry``
   * This is the FIRST instruction executed when QEMU loads the kernel
   * ``_entry`` is defined in ``boot/entry.S`` at address **0x80000000**
   * QEMU's ``-bios none`` mode jumps directly to this address in M-mode
   * Debuggers (GDB) use this to set breakpoints at boot

**Why _entry, not _start?**

ThunderOS has a two-stage boot process:

1. **M-mode entry** (``_entry`` in ``entry.S`` at 0x80000000) - Machine mode initialization
2. **S-mode boot** (``_start`` in ``boot.S`` at 0x80004020) - Supervisor mode setup

The linker's ``ENTRY()`` directive points to the very first instruction, which is ``_entry``.

**Verification:**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -h build/thunderos.elf | grep Entry
   Entry point address:               0x80000000
   
   $ riscv64-unknown-elf-nm build/thunderos.elf | grep -E "^80000000|^80004020"
   0000000080000000 T _entry
   0000000080004020 T _start

3. Base Address: 0x80000000
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   . = 0x80000000;

**The Location Counter ('.'):**

The dot (`.`) is a special variable in linker scripts representing the **current address**. Assigning to it sets where the next section will be placed.

**Why 0x80000000?**

This is dictated by the QEMU RISC-V ``virt`` machine with ``-bios none``:

* **Without firmware** (``-bios none``): QEMU jumps directly to **0x80000000** in M-mode
* **With OpenSBI** (``-bios default``): Firmware at 0x80000000, kernel would be at 0x80200000

ThunderOS uses ``-bios none`` to have complete control over M-mode initialization.

**QEMU virt Machine Memory Map:**

.. code-block:: text

   ┌──────────────────┬──────────┬──────────────────────────────────────┐
   │ Address Range    │ Size     │ Description                          │
   ├──────────────────┼──────────┼──────────────────────────────────────┤
   │ 0x00001000       │ 4KB      │ Boot ROM (reset vector points here)  │
   │ 0x02000000       │ 64KB     │ CLINT (Core Local Interruptor)       │
   │ 0x0C000000       │ 64MB     │ PLIC (Platform Interrupt Controller) │
   │ 0x10000000       │ 256B     │ UART0 (NS16550A serial console)      │
   │ 0x10001000+      │ varies   │ VirtIO MMIO devices (block, GPU)     │
   ├──────────────────┼──────────┼──────────────────────────────────────┤
   │ 0x80000000       │ ~330KB   │ **ThunderOS Kernel**                 │
   │   _entry (M)     │          │   M-mode entry point                 │
   │   _start (S)     │          │   S-mode boot                        │
   │   kernel_main    │          │   Main kernel                        │
   │   ...            │          │   All kernel code/data               │
   │   _kernel_end    │          │   End marker (0x80055000)            │
   ├──────────────────┼──────────┼──────────────────────────────────────┤
   │ 0x80055000 -     │ ~127.7MB │ **Free RAM**                         │
   │ 0x88000000       │          │   Used by PMM, DMA, user processes   │
   └──────────────────┴──────────┴──────────────────────────────────────┘
   
   Total RAM: 128MB (0x80000000 - 0x88000000)

**Boot Sequence:**

.. code-block:: text

   QEMU Start (with -bios none)
         ↓
   PC ← 0x80000000 (hardware reset vector)
   Mode: M-mode (Machine Mode)
         ↓
   _entry (entry.S) - First instruction of ThunderOS
         ↓
   M-mode initialization (CSRs, PMP, timer)
         ↓
   mret → _start (boot.S) - Transition to S-mode
         ↓
   S-mode initialization (clear BSS, setup stack)
         ↓
   call kernel_main - Enter C code

**Why Can't We Use a Different Address?**

We *could*, but:

* 0x80000000 is the standard RISC-V DRAM base address
* QEMU ``virt`` machine expects kernel here with ``-bios none``
* Changing it requires modifying QEMU command line or using custom firmware
* Following conventions makes the OS more portable

4. Text Section (.text)
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .text : {
       PROVIDE(_text_start = .);
       *(.text.entry)     /* M-mode entry from entry.S */
       *(.text.boot)      /* S-mode boot from boot.S */
       *(.text .text.*)
       PROVIDE(_text_end = .);
   } :text

**Purpose:** Contains all executable code (functions, procedures)

**Section Ordering - CRITICAL:**

The order of sections within ``.text`` matters for boot:

1. **``.text.entry``** - MUST be first
   
   * Contains ``_entry`` symbol from ``boot/entry.S``
   * Located at 0x80000000 (first byte of kernel)
   * First instruction QEMU executes
   * Handles M-mode initialization

2. **``.text.boot``** - Second
   
   * Contains ``_start`` symbol from ``boot/boot.S``
   * Located at 0x80004020 (after entry code)
   * Handles S-mode initialization
   * Clears BSS, sets up stack

3. **``.text`` and ``.text.*``** - Everything else
   
   * All other functions: ``kernel_main()``, ``uart_init()``, syscalls, etc.
   * Compiler-generated function sections (``-ffunction-sections``)
   * Order doesn't matter for these

**How Sections Are Specified in Source:**

In assembly (``entry.S``):

.. code-block:: asm

   .section .text.entry
   .globl _entry
   _entry:
       # First instruction at 0x80000000
       csrw sie, zero
       ...

In C (most kernel code):

.. code-block:: c

   // Automatically goes in .text
   void kernel_main(void) {
       ...
   }

**Program Header Assignment:**

The ``:text`` at the end assigns this section to the ``text`` program header (R+X permissions).

**Symbols Provided:**

* ``_text_start`` (0x80000000) - Start of code section
* ``_text_end`` (0x800239b2) - End of code section

These can be used in C code:

.. code-block:: c

   extern char _text_start[], _text_end[];
   
   size_t code_size = _text_end - _text_start;
   uart_printf("Kernel code: %zu bytes\n", code_size);

**Actual Contents:**

.. code-block:: text

   Address        Function         File
   ─────────────────────────────────────────────────
   0x80000000     _entry           boot/entry.S
   0x80004020     _start           boot/boot.S
   0x80004048     clear_bss_done   boot/boot.S
   0x800052dc     hal_uart_...     kernel/drivers/...
   0x800054fe     process_entry    kernel/core/process.c
   ...
   0x800239b2     (end of .text)

5. Read-Only Data Section (.rodata)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .rodata : {
       PROVIDE(_rodata_start = .);
       *(.rodata .rodata.*)
       PROVIDE(_rodata_end = .);
   } :text

**Purpose:** Contains constant data that should never be modified

**What Goes Here:**

* String literals: ``"ThunderOS v1.0"``
* ``const`` variables: ``const int MAX_PROCS = 64;``
* Lookup tables: ``const uint8_t sin_table[256] = {...};``
* Function pointers marked const
* Compiler-generated constants

**Why Separate from .text?**

While both are read-only and executable, separating them:

* Makes the code more organized
* Helps with debugging (easier to find string constants)
* Allows for different caching strategies (future optimization)

**Why in the :text Program Header?**

The ``.rodata`` section is assigned to the same program header as ``.text`` (R+X permissions). This means:

* It's in the same memory segment as code
* It gets the same permissions: Read + Execute
* Cannot be modified at runtime (hardware enforced once MMU is active)

**Wait, Execute Permission on Data?**

This seems odd, but it's acceptable because:

* The data is constant and verified at compile time
* No self-modifying code is generated
* Simplifies the loader (one segment for all read-only content)
* Some architectures (like x86) do this for small performance gains

Modern systems sometimes use a third segment (R only) for ``.rodata``, but for ThunderOS's purposes, R+X is fine.

**Example Contents:**

.. code-block:: c

   // These all end up in .rodata:
   const char *kernel_version = "ThunderOS v0.1.0";
   const int MAX_OPEN_FILES = 256;
   const uint32_t magic_numbers[] = {0xDEADBEEF, 0xCAFEBABE};

**Attempting to Modify (will crash when MMU is active):**

.. code-block:: c

   extern const char *kernel_version;
   
   // This compiles but will trigger a page fault once MMU is enabled:
   kernel_version[0] = 'X';  // Attempt to write to read-only memory
   // Result: Hardware exception → kernel panic

6. Page Alignment for Memory Protection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   . = ALIGN(4096);

**Purpose:** Align the next section to a 4KB (4096-byte) page boundary

**Why Is This Critical?**

Memory protection in RISC-V (and most architectures) works at **page granularity**:

* A **page** is the smallest unit of memory that can have independent permissions
* RISC-V standard page size: **4KB (4096 bytes)**
* Page table entries control permissions for entire 4KB pages
* Cannot have different permissions within a single page

**The Problem Without Alignment:**

.. code-block:: text

   Without ALIGN(4096):
   
   0x800239b2  ← .text ends (R+X permissions)
   0x800239b3  ← .data starts (R+W permissions)
   
   Both are in the same 4KB page (0x80023000 - 0x80023FFF)!
   Cannot have both R+X and R+W for the same page!

**The Solution With Alignment:**

.. code-block:: text

   With ALIGN(4096):
   
   0x800239b2  ← .text ends
   0x800239b3  ← padding
   0x800239b4  ← padding
   ...
   0x80023FFF  ← end of page
   0x80024000  ← .data starts (new page with R+W permissions)
   
   Clean separation: Different pages = Different permissions!

**The Trade-Off:**

* **Cost**: Wastes space (up to 4095 bytes of padding)
* **Benefit**: Essential for memory protection
* **Result**: In ThunderOS, only ~622 bytes wasted (page boundary was close)

**Why 4096?**

This is the RISC-V Sv39 page size. Other architectures use:

* x86-64: 4KB (4096 bytes)
* ARM: 4KB or 16KB or 64KB
* Some systems: 2MB or 1GB "huge pages" for performance

**Verification:**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -S build/thunderos.elf
   
   [Nr] Name       Type      Address          Off    Size
   [ 1] .text      PROGBITS  0000000080000000 001000 0239b2
   [ 2] .rodata    PROGBITS  00000000800239c0 0249c0 000040
   [ 3] .data      PROGBITS  0000000080024000 025000 0000d4  ← Aligned!
   
   0x80024000 is exactly divisible by 4096 (0x1000)

7. Data Section (.data)
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .data : {
       PROVIDE(_data_start = .);
       *(.data .data.*)
       PROVIDE(_data_end = .);
   } :data

**Purpose:** Contains initialized global and static variables

**What Goes Here:**

.. code-block:: c

   // All of these go in .data:
   int global_counter = 42;
   static char buffer[256] = "initialized";
   struct config cfg = {.version = 1, .debug = true};

**vs .rodata:**

.. code-block:: c

   const int readonly = 100;  // → .rodata (cannot modify)
   int readwrite = 100;       // → .data (can modify)

**vs .bss:**

.. code-block:: c

   int initialized = 5;    // → .data (stored in binary)
   int uninitialized;      // → .bss (not stored, zeroed at boot)

**Program Header Assignment:**

The ``:data`` at the end assigns this to the ``data`` program header (R+W permissions):

* Can be read
* Can be written
* CANNOT be executed (W^X protection)

**Size in ThunderOS:**

Currently very small (212 bytes) because:

* Most kernel data is zero-initialized (goes in .bss)
* Kernel uses mostly stack and dynamically allocated memory
* Few global variables need explicit initialization

**Symbols Provided:**

* ``_data_start`` (0x80024000)
* ``_data_end`` (0x800240d4)

**Storage in Binary:**

The ``.data`` section MUST be stored in the ELF file because it contains specific values:

.. code-block:: text

   thunderos.elf file structure:
   
   [File Header]
   [Program Headers]
   [.text section]        ← 145KB of code
   [.rodata section]      ← String constants
   [.data section]        ← 212 bytes of initialized values
   [Symbol Tables]
   ...

When loaded, these bytes are copied from the file to memory at 0x80024000.

8. BSS Section (.bss)
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .bss : {
       PROVIDE(_bss_start = .);
       *(.bss .bss.*)
       *(COMMON)
       PROVIDE(_bss_end = .);
   } :data

**Purpose:** Contains zero-initialized variables

**BSS = "Block Started by Symbol"** (historical IBM assembler term)

**What Goes Here:**

All **uninitialized** (or explicitly zero-initialized) global and static variables:

.. code-block:: c

   // All of these go in .bss (or .sbss if small):
   int uninitialized_global;                    // → .bss (no initializer)
   static char large_buffer[1024 * 1024];       // → .bss (large, no init)
   struct process processes[MAX_PROCESSES];     // → .bss (array, no init)
   int explicitly_zero = 0;                     // → .bss (optimized to BSS)
   static int counter;                          // → .bss (static, no init)

**vs .data:**

.. code-block:: c

   int initialized = 42;                        // → .data (has initializer)
   static char buffer[256] = "hello";           // → .data (initialized string)

**Small vs Regular BSS:**

* Variables **≤8 bytes** may go in ``.sbss`` (small BSS, GP-relative addressing)
* Larger variables go in regular ``.bss``
* Both are zero-initialized; the difference is just addressing mode optimization

**The Key Optimization:**

BSS data is **NOT stored in the binary file**. Instead:

1. Linker records only the SIZE of .bss (188KB)
2. Binary file doesn't contain 188KB of zeros
3. At boot, ``boot.S`` zeros this memory region
4. Saves disk space and load time

**In ThunderOS:**

.. code-block:: asm

   # From boot/boot.S:
   clear_bss:
       la t0, _bss_start
       la t1, _bss_end
   1:
       sd zero, 0(t0)   # Write 8 bytes of zeros
       addi t0, t0, 8
       bne t0, t1, 1b   # Loop until done

**Why Zero-Initialize?**

* **C standard requires it**: All global/static variables without initializers must be zero
* **Security**: Prevents information leaks (old memory contents)
* **Predictability**: Programs can rely on zero initialization

**COMMON Section:**

.. code-block:: ld

   *(COMMON)

This handles **tentative definitions** - a quirky C feature:

.. code-block:: c

   // In file1.c:
   int shared_var;  // Tentative definition
   
   // In file2.c:
   int shared_var;  // Another tentative definition
   
   // Linker merges these into ONE variable in COMMON/.bss

Modern code should avoid this (use ``extern`` properly), but the linker script handles it for compatibility.

**Size in ThunderOS:**

188KB (192,552 bytes) - Much larger than .data (212 bytes) because:

* **Stack space**: 16KB M-mode stack + 16KB S-mode stack
* **Process table arrays**: ``process_t processes[MAX_PROCESSES]``
* **Memory management structures**: Page tables, free lists
* **Large buffers**: VirtIO buffers, DMA buffers
* **All uninitialized globals**: Variables with no initializer

Most kernel data is uninitialized (zero is fine), so BSS is much larger than .data.

**Small Data Sections (.sdata and .sbss):**

RISC-V supports **GP-relative addressing** for small data:

* **GP register** (x3) holds a base address
* Small variables (≤8 bytes) can be accessed with single instructions
* ``lw a0, offset(gp)`` instead of ``lui + addi + lw`` (saves instructions)
* Compiler automatically puts small globals in ``.sdata`` (initialized) or ``.sbss`` (uninitialized)
* ThunderOS currently has very few small globals (just a few bytes each)

**Example:**

.. code-block:: c

   // These might go in .sdata/.sbss:
   int small_counter;           // 4 bytes → .sbss
   void *small_ptr;             // 8 bytes → .sbss
   
   // These go in regular .data/.bss:
   char large_buffer[4096];     // 4KB → .bss
   struct config settings;      // Large struct → .bss

**Symbols Provided:**

* ``_bss_start`` (0x80025000) - Start of regular BSS
* ``_bss_end`` (0x80054020) - End of all BSS (includes .sbss)

9. Kernel End Marker
~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   . = ALIGN(4096);
   PROVIDE(_kernel_end = .);

**Purpose:** Mark the end of the kernel in memory

**Why Align Again?**

Ensures ``_kernel_end`` is on a page boundary. Benefits:

* Free memory starts at a page boundary
* PMM (Physical Memory Manager) can immediately use it
* No partial page to handle
* Cleaner memory management

**What is _kernel_end Used For?**

The Physical Memory Manager (PMM) uses this to know where free RAM begins:

.. code-block:: c

   // From kernel/mm/pmm.c:
   extern char _kernel_end[];
   
   void pmm_init(void) {
       // Free memory starts after kernel
       void *free_start = (void*)_kernel_end;
       
       // RAM ends at 128MB mark
       void *free_end = (void*)0x88000000;
       
       // Everything between is available for allocation
       pmm_add_free_region(free_start, free_end - free_start);
   }

**Memory Layout After Boot:**

.. code-block:: text

   ┌─────────────────────┬──────────┬──────────────────────┐
   │ Address             │ Size     │ Usage                │
   ├─────────────────────┼──────────┼──────────────────────┤
   │ 0x80000000          │ ~330KB   │ ThunderOS kernel     │
   │                     │          │  (code + data + bss) │
   ├─────────────────────┼──────────┼──────────────────────┤
   │ 0x80055000          │          │ ← _kernel_end        │
   │   (_kernel_end)     │          │                      │
   ├─────────────────────┼──────────┼──────────────────────┤
   │ 0x80055000 -        │ ~127.7MB │ Free memory managed  │
   │ 0x88000000          │          │ by PMM:              │
   │                     │          │  - DMA buffers       │
   │                     │          │  - Process memory    │
   │                     │          │  - Kernel heap       │
   │                     │          │  - Page tables       │
   └─────────────────────┴──────────┴──────────────────────┘

**Actual Value:**

.. code-block:: bash

   $ riscv64-unknown-elf-nm build/thunderos.elf | grep _kernel_end
   0000000080055000 B _kernel_end

10. Custom Section: User Exception Test
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   .user_exception_test : {
       user_exception_start = .;
       *(.user_exception_test)
       user_exception_end = .;
   } :data

**Purpose:** Embed a test binary directly in the kernel

**How It Works:**

In C code, data can be placed in this section:

.. code-block:: c

   // Compiled test program embedded in kernel
   const uint8_t __attribute__((section(".user_exception_test")))
       test_binary[] = {
       0x37, 0x05, 0x00, 0x00,  // lui a0, 0x0
       0x13, 0x05, 0x05, 0x05,  // addi a0, a0, 0x50
       // ... more instructions
   };

**Why Embed Test Programs?**

* No need for separate filesystem during early testing
* Test binary always available
* Can test user mode execution before VFS is ready
* Simplifies testing and development

**Symbols Provided:**

* ``user_exception_start`` - Start of embedded test
* ``user_exception_end`` - End of embedded test

The kernel can use these to load and execute the test:

.. code-block:: c

   extern char user_exception_start[], user_exception_end[];
   
   size_t test_size = user_exception_end - user_exception_start;
   memcpy(user_memory, user_exception_start, test_size);
   exec_user_program(user_memory);

11. Discarded Sections
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   /DISCARD/ : {
       *(.comment)
       *(.eh_frame)
   }

**Purpose:** Remove unnecessary sections from the final binary

**.comment Section:**

* Contains compiler version strings
* Build timestamps and metadata
* Example: "GCC: (GNU) 11.1.0"
* Useful for debugging but not needed at runtime
* Discarding saves ~100-200 bytes

**.eh_frame Section:**

* **Exception Handling Frame** information
* Used by C++ for stack unwinding during exceptions
* Allows debuggers to reconstruct call stacks
* ThunderOS doesn't use C++ exceptions
* Discarding saves significant space (10-50KB)

**Why Discard?**

* **Smaller binary**: Faster loading, less memory usage
* **Cleaner memory layout**: Only essential data in kernel
* **No runtime cost**: These sections are debug/language features we don't use

**What About Debugging?**

Debug information is in separate sections (``.debug_*``) which are automatically not loaded into memory. They exist only in the ELF file for debuggers.

**Other Sections Sometimes Discarded:**

.. code-block:: ld

   /DISCARD/ : {
       *(.note*)          /* Build notes */
       *(.gcc_except_table)  /* GCC exception tables */
       *(.ARM.exidx*)     /* ARM-specific (not used on RISC-V) */
   }

ThunderOS keeps it simple, discarding only what's clearly unnecessary. (.rodata)
ThunderOS keeps it simple, discarding only what's clearly unnecessary.

Symbols Provided to Code
-------------------------

The linker script uses ``PROVIDE()`` to create symbols that can be accessed from C and assembly code. These symbols mark important boundaries in memory.

Symbol Table
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 20 55

   * - Symbol
     - Address
     - Description
   * - ``_text_start``
     - 0x80000000
     - First byte of executable code
   * - ``_text_end``
     - 0x800239b2
     - Last byte of code section
   * - ``_rodata_start``
     - 0x800239c0
     - First byte of read-only data
   * - ``_rodata_end``
     - 0x80023a00
     - Last byte of constants
   * - ``_data_start``
     - 0x80024000
     - First byte of initialized data
   * - ``_data_end``
     - 0x800240d4
     - Last byte of initialized variables
   * - ``_bss_start``
     - 0x80025000
     - First byte of zero-initialized data
   * - ``_bss_end``
     - 0x80054020
     - Last byte of BSS section
   * - ``_kernel_end``
     - 0x80055000
     - End of entire kernel image
   * - ``user_exception_start``
     - 0x80055000
     - Start of embedded test binary
   * - ``user_exception_end``
     - varies
     - End of embedded test binary

Using Symbols in C Code
~~~~~~~~~~~~~~~~~~~~~~~~

**Important:** Linker symbols represent **addresses**, not variables!

**WRONG Way:**

.. code-block:: c

   extern int _bss_start;  // ❌ Declares as integer variable
   extern int _bss_end;
   
   int size = _bss_end - _bss_start;  // ❌ Reads garbage values!

   int size = _bss_end - _bss_start;  // ❌ Reads garbage values!

**CORRECT Way (Method 1 - Array Declaration):**

.. code-block:: c

   extern char _bss_start[];  // ✅ Declare as array
   extern char _bss_end[];
   
   size_t size = _bss_end - _bss_start;  // ✅ Pointer arithmetic

**CORRECT Way (Method 2 - Address-Of):**

.. code-block:: c

   extern char _bss_start;  // Declare as char
   extern char _bss_end;
   
   size_t size = &_bss_end - &_bss_start;  // ✅ Take address first

**Real Usage Example from boot.S:**

.. code-block:: asm

   # Clear BSS section at boot
   clear_bss:
       la t0, _bss_start      # Load address of _bss_start
       la t1, _bss_end        # Load address of _bss_end
   1:
       sd zero, 0(t0)         # Store 8 zero bytes
       addi t0, t0, 8         # Advance pointer
       bne t0, t1, 1b         # Loop until done

**Real Usage Example from PMM:**

.. code-block:: c

   // From kernel/mm/pmm.c
   extern char _kernel_end[];
   
   void pmm_init(void) {
       uintptr_t free_start = (uintptr_t)_kernel_end;
       uintptr_t free_end = 0x88000000;  // 128MB RAM limit
       
       size_t free_size = free_end - free_start;
       uart_printf("Free RAM: %zu MB\n", free_size / (1024 * 1024));
       
       pmm_add_free_region((void*)free_start, free_size);
   }

**Debugging/Info Functions:**

.. code-block:: c

   void print_memory_layout(void) {
       extern char _text_start[], _text_end[];
       extern char _rodata_start[], _rodata_end[];
       extern char _data_start[], _data_end[];
       extern char _bss_start[], _bss_end[];
       extern char _kernel_end[];
       
       uart_printf("Memory Layout:\n");
       uart_printf("  .text:   %p - %p (%zu KB)\n",
           _text_start, _text_end,
           (_text_end - _text_start) / 1024);
       
       uart_printf("  .rodata: %p - %p (%zu bytes)\n",
           _rodata_start, _rodata_end,
           _rodata_end - _rodata_start);
       
       uart_printf("  .data:   %p - %p (%zu bytes)\n",
           _data_start, _data_end,
           _data_end - _data_start);
       
       uart_printf("  .bss:    %p - %p (%zu KB)\n",
           _bss_start, _bss_end,
           (_bss_end - _bss_start) / 1024);
       
       uart_printf("  Kernel ends at: %p\n", _kernel_end);
   }

Section Attributes in C/Assembly
---------------------------------

You can control section placement directly from source code using compiler attributes.

Placing Functions in Specific Sections
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In C:**

.. code-block:: c

   // Place in .text.boot (early boot code)
   void __attribute__((section(".text.boot"))) 
   early_init(void) {
       // Runs before main kernel
       // Must not use BSS (not zeroed yet)
   }
   
   // Place in .text.entry (very first code)
   void __attribute__((section(".text.entry"), noreturn))
   _entry(void) {
       // First function executed
       // In practice, this is written in assembly (entry.S)
   }

**In Assembly:**

.. code-block:: asm

   # From boot/entry.S
   .section .text.entry
   .globl _entry
   .type _entry, @function
   _entry:
       # First instruction at 0x80000000
       csrw sie, zero
       ...

Placing Data in Specific Sections
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Force into .data (even if zero-initialized)
   int __attribute__((section(".data"))) 
   important_counter = 0;
   
   // Create custom section for embedded binary
   const uint8_t __attribute__((section(".user_exception_test")))
   test_program[] = {
       0x37, 0x05, 0x00, 0x00,  // RISC-V instructions
       0x13, 0x05, 0x05, 0x05,
       // ...
   };
   
   // Read-only configuration
   const struct config __attribute__((section(".rodata")))
   kernel_config = {
       .version = 1,
       .max_procs = 64,
       .debug = true
   };

Alignment Attributes
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Align to cache line (64 bytes)
   char __attribute__((aligned(64))) 
   dma_buffer[4096];
   
   // Align to page boundary (4KB)
   char __attribute__((aligned(4096)))
   page_table[4096];
   
   // Pack structure (no padding)
   struct __attribute__((packed)) virtio_desc {
       uint64_t addr;
       uint32_t len;
       uint16_t flags;
       uint16_t next;
   };

Memory Layout Visualization
----------------------------

Complete Memory Map
~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Physical Address    Section          Size      Permissions  Contents
   ════════════════════════════════════════════════════════════════════════════
   0x80000000         ┌──────────────┐
                      │ .text.entry  │  ~16KB    R+X          M-mode entry
                      │              │                        (_entry)
   0x80004020         ├──────────────┤
                      │ .text.boot   │  ~48B     R+X          S-mode boot
                      │              │                        (_start)
   0x80004048         ├──────────────┤
                      │ .text        │  ~145KB   R+X          Kernel code
                      │              │                        (functions)
   0x800239b2         ├──────────────┤
   (0x800239c0)       │ (padding)    │  ~14B     R+X          (alignment)
                      ├──────────────┤
                      │ .rodata      │  ~64B     R+X          String constants
   0x80023a00         └──────────────┘
                      
   0x80024000         ┌──────────────┐           ← Page boundary (4KB aligned)
                      │ .data        │  212B     R+W          Initialized vars
   0x800240d4         ├──────────────┤
                      │ .sdata       │  ~few B   R+W          Small data (GP-relative)
                      │              │                        Variables ≤8 bytes accessed
                      │              │                        via global pointer (gp)
                      ├──────────────┤
   0x80025000         │ .bss         │  ~188KB   R+W          Uninitialized vars
                      │              │                        (zero-initialized at boot)
                      │              │                        Stacks, buffers, arrays
   0x80054020         ├──────────────┤
                      │ .sbss        │  ~few B   R+W          Small BSS (GP-relative)
                      │              │                        Uninitialized vars ≤8 bytes
                      └──────────────┘
   0x80055000         ═══════════════            ← _kernel_end (page aligned)
                      
                      ┌──────────────┐           ← Free memory starts
                      │              │
                      │   Free RAM   │  ~127MB   (managed)   PMM, DMA, processes
                      │              │                        page tables, heap
                      │              │
   0x88000000         └──────────────┘           ← End of 128MB RAM

Section Sizes (from riscv64-unknown-elf-size)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Section    Size (bytes)   Size (KB)    Percentage
   ───────────────────────────────────────────────────
   .text         145,842       142.4KB        43.1%
   .rodata        (in text)    (in text)
   .data             212         0.2KB         0.06%
   .sdata         (small)      (small)
   .bss          192,552       188.0KB        56.9%
   .sbss          (small)      (small)
   ───────────────────────────────────────────────────
   Total         338,606       330.7KB       100%

Binary File vs Memory
~~~~~~~~~~~~~~~~~~~~~

**ELF File on Disk (thunderos.elf):**

.. code-block:: text

   [ELF Header]                  64 bytes
   [Program Headers]             ~200 bytes
   [.text section]               145,842 bytes  ← Stored in file
   [.rodata section]             ~64 bytes      ← Stored in file
   [.data section]               212 bytes      ← Stored in file
   [.bss - SIZE ONLY]            (not stored!)  ← Only size recorded
   [Symbol Table]                ~10KB
   [String Table]                ~5KB
   [Debug Info]                  ~100KB (if compiled with -g)
   
   Total file size: ~160KB (without .bss data!)

**Loaded Into Memory:**

.. code-block:: text

   0x80000000:  [.text]    145,842 bytes  ← Copied from file
   0x800239c0:  [.rodata]      ~64 bytes  ← Copied from file
   0x80024000:  [.data]        212 bytes  ← Copied from file
   0x80025000:  [.bss]     192,552 bytes  ← Zeroed by boot.S
   
   Total memory used: 338,606 bytes

The .bss optimization saves ~188KB in the file!

Checking the Memory Layout
---------------------------

Useful Commands
~~~~~~~~~~~~~~~

**1. View section addresses and sizes:**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -S build/thunderos.elf
   
   Section Headers:
   [Nr] Name       Type      Address          Off    Size   ES Flg Lk Inf Al
   [ 1] .text      PROGBITS  0000000080000000 001000 0239b2 00  AX  0   0  4
   [ 2] .rodata    PROGBITS  00000000800239c0 0249c0 000040 00   A  0   0  8
   [ 3] .data      PROGBITS  0000000080024000 025000 0000d4 00  WA  0   0  8
   [ 4] .bss       NOBITS    0000000080025000 0250d4 02f028 00  WA  0   0  8

**2. View program headers (segments):**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -l build/thunderos.elf
   
   Program Headers:
     Type      Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg
     LOAD      0x001000 0x0000000080000000 0x0000000080000000 0x0239b2 0x0239b2 R E
     LOAD      0x025000 0x0000000080024000 0x0000000080024000 0x0000d4 0x030028 RW

**3. View symbol addresses:**

.. code-block:: bash

   $ riscv64-unknown-elf-nm build/thunderos.elf | grep -E "_(text|data|bss|rodata|kernel_end)"
   
   0000000080000000 T _text_start
   00000000800239b2 T _text_end
   00000000800239c0 R _rodata_start
   0000000080023a00 R _rodata_end
   0000000080024000 D _data_start
   00000000800240d4 D _data_end
   0000000080025000 B _bss_start
   0000000080054020 B _bss_end
   0000000080055000 B _kernel_end

**4. Check section sizes:**

.. code-block:: bash

   $ riscv64-unknown-elf-size build/thunderos.elf
      text    data     bss     dec     hex filename
    145842     212  192552  338606   52aae build/thunderos.elf

**5. View entry point:**

.. code-block:: bash

   $ riscv64-unknown-elf-readelf -h build/thunderos.elf | grep Entry
     Entry point address:               0x80000000

**6. Generate map file (during linking):**

.. code-block:: bash

   $ riscv64-unknown-elf-ld -Map=kernel.map -T kernel.ld ...
   $ cat kernel.map  # Shows detailed memory layout

Common Issues and Solutions
----------------------------

1. Section Overlap
~~~~~~~~~~~~~~~~~~

**Error:**

.. code-block:: text

   section .data VMA [0x80023000, 0x80024000] overlaps section .text VMA [0x80000000, 0x80024500]

**Cause:** Sections are running into each other (not enough space or missing alignment)

**Solution:**

* Add ``ALIGN(4096)`` between sections
* Increase base address spacing
* Check for very large arrays/buffers

.. code-block:: ld

   .text : { ... }
   . = ALIGN(4096);  /* Force gap */
   .data : { ... }

2. Wrong Entry Point
~~~~~~~~~~~~~~~~~~~~

**Error:** Kernel doesn't boot, hangs, or crashes immediately

**Symptoms:**

* QEMU shows no output
* GDB shows PC at wrong address
* Illegal instruction trap

**Causes & Solutions:**

.. code-block:: ld

   # WRONG:
   ENTRY(_start)         # _start is at 0x80004020, not 0x80000000!
   
   # CORRECT:
   ENTRY(_entry)         # _entry is at 0x80000000 (first instruction)

Also verify in assembly:

.. code-block:: asm

   # Must be global and in .text.entry
   .section .text.entry
   .globl _entry         # ← MUST be global!
   _entry:
       ...

3. Undefined Reference to Linker Symbols
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Error:**

.. code-block:: text

   undefined reference to `_bss_start'

**Cause:** Symbol not provided in linker script

**Solution:** Add ``PROVIDE()`` directive:

.. code-block:: ld

   .bss : {
       PROVIDE(_bss_start = .);  /* ← Add this */
       *(.bss)
       PROVIDE(_bss_end = .);    /* ← And this */
   }

4. Alignment Faults
~~~~~~~~~~~~~~~~~~~

**Error:**

.. code-block:: text

   Load address misaligned
   Store/AMO address misaligned

**Cause:** Data not aligned to natural boundaries (RISC-V requires aligned access)

**Solutions:**

In linker script:

.. code-block:: ld

   .data : ALIGN(8) {  /* Align to 8-byte boundary */
       *(.data)
   }

In C code:

.. code-block:: c

   struct __attribute__((aligned(8))) my_struct {
       uint64_t value;
   };

5. BSS Not Cleared
~~~~~~~~~~~~~~~~~~

**Symptom:** Variables contain garbage instead of zero

**Cause:** Boot code isn't clearing BSS properly

**Verify** ``boot.S`` clears BSS:

.. code-block:: asm

   clear_bss:
       la t0, _bss_start
       la t1, _bss_end
   1:  sd zero, 0(t0)
       addi t0, t0, 8
       blt t0, t1, 1b    # Loop while t0 < t1

**Debug:** Print in early boot:

.. code-block:: c

   extern char _bss_start[], _bss_end[];
   
   // Check if BSS was cleared
   for (char *p = _bss_start; p < _bss_end; p++) {
       if (*p != 0) {
           uart_printf("BSS not cleared at %p!\n", p);
       }
   }

6. Running Out of Memory
~~~~~~~~~~~~~~~~~~~~~~~~~

**Error:**

.. code-block:: text

   region `ram' overflowed by 1024 bytes

**Causes:**

* Kernel too large (too much code/data)
* Very large static arrays
* Stack sizes too big

**Solutions:**

* Use dynamic allocation instead of static arrays
* Reduce stack sizes
* Enable compiler optimizations (``-Os`` or ``-O2``)
* Check for bloat (unused code/libraries)

.. code-block:: bash

   # Find largest symbols:
   $ riscv64-unknown-elf-nm --size-sort -S build/thunderos.elf | tail -20

Advanced Topics
---------------

Multiple Memory Regions
~~~~~~~~~~~~~~~~~~~~~~~

For systems with distinct RAM/ROM regions:

.. code-block:: ld

   MEMORY {
       ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
       RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
   }
   
   SECTIONS {
       .text : {
           *(.text*)
       } > ROM  /* Load in ROM */
       
       .data : {
           *(.data*)
       } > RAM AT> ROM  /* Load from ROM, run in RAM */
   }

The ``AT>`` directive means "load from ROM but execute in RAM" (useful for data that needs to be copied at boot).

Load vs Execution Addresses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Different load and execution addresses (used in bootloaders):

.. code-block:: ld

   .data 0x80000000 : AT(0x00100000) {
       *(.data)
   }

* **VMA (Virtual Memory Address)**: 0x80000000 (where code expects it)
* **LMA (Load Memory Address)**: 0x00100000 (where it's stored in ROM)
* Boot code must copy from LMA to VMA

Custom Sections for Special Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ld

   /* Section for AI model weights */
   .ai_models : {
       PROVIDE(__ai_models_start = .);
       *(.ai_models)
       PROVIDE(__ai_models_end = .);
   }
   
   /* Section for interrupt vectors */
   .vectors : {
       KEEP(*(.vectors))  /* Don't garbage collect */
   } > ROM

Then in C:

.. code-block:: c

   const float __attribute__((section(".ai_models")))
   neural_weights[1000000] = { /* ... */ };

Section Garbage Collection
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Remove unused functions to save space:

**Compile with:**

.. code-block:: bash

   -ffunction-sections -fdata-sections

**Link with:**

.. code-block:: bash

   --gc-sections

**Linker script:**

.. code-block:: ld

   .text : {
       /* Keep entry point */
       KEEP(*(.text.entry))
       KEEP(*(.text.boot))
       
       /* Rest can be garbage collected */
       *(.text*)
   }

Debugging Linker Issues
~~~~~~~~~~~~~~~~~~~~~~~

**Verbose output:**

.. code-block:: bash

   riscv64-unknown-elf-ld --verbose ...

**Generate map file:**

.. code-block:: bash

   riscv64-unknown-elf-ld -Map=kernel.map ...

**Check specific symbol:**

.. code-block:: bash

   grep symbol_name kernel.map

**View why a section was kept:**

.. code-block:: bash

   --print-gc-sections

See Also
--------

* :doc:`bootloader` - Uses _bss_start/_bss_end symbols
* :doc:`memory` - Complete memory management and layout
* `GNU ld Manual <https://sourceware.org/binutils/docs/ld/>`_
