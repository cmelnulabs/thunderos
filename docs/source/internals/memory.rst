Memory Management
=================

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

ThunderOS implements comprehensive memory management including:

1. **Physical Memory Layout** - How RAM is organized on QEMU virt machine
2. **Virtual Memory (Paging)** - Sv39 page tables for address translation
3. **Memory Isolation** - Per-process address spaces with VMAs
4. **Memory Allocators** - PMM (physical) and kmalloc (kernel heap)

This document covers the layout and isolation aspects. See :doc:`paging`, :doc:`pmm`, and :doc:`kmalloc` for allocator details.

Physical Memory Layout
----------------------

QEMU Virt Machine Memory Map
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The RISC-V ``virt`` machine in QEMU has the following memory-mapped devices:

.. code-block:: text

   ┌──────────────────┬─────────────┬────────────────────────────┐
   │ Address Range    │ Size        │ Device                     │
   ├──────────────────┼─────────────┼────────────────────────────┤
   │ 0x00000000       │ 256B        │ Test device                │
   │ 0x00001000       │ 4KB         │ Boot ROM (reset vector)    │
   │ 0x00100000       │ 4KB         │ RTC (Real Time Clock)      │
   │ 0x02000000       │ 64KB        │ CLINT (timer interrupts)   │
   │ 0x0C000000       │ 4MB         │ PLIC (interrupt controller)│
   │ 0x10000000       │ 256B        │ UART0 (serial console)     │
   │ 0x10001000       │ 4KB         │ VirtIO (disk, network, etc)│
   │ 0x40000000       │ 1GB         │ PCIe MMIO (future)         │
   │ 0x80000000       │ 128MB       │ **RAM** (kernel lives here)│
   └──────────────────┴─────────────┴────────────────────────────┘

Kernel Memory Organization
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   0x80000000  ┌─────────────────────────────────┐
               │ Kernel Sections                 │
               │ ┌─────────────────────────────┐ │
               │ │ .text.boot (entry point)    │ │ ← _start
               │ ├─────────────────────────────┤ │
               │ │ .text (kernel code)         │ │
               │ ├─────────────────────────────┤ │
               │ │ .rodata (constants)         │ │
               │ ├─────────────────────────────┤ │
               │ │ .data (initialized globals) │ │
               │ ├─────────────────────────────┤ │
               │ │ .bss (uninit + stack)       │ │
               │ └─────────────────────────────┘ │
   ~0x80025000 ├─────────────────────────────────┤ ← _kernel_end
               │                                 │
               │ Free RAM (~127MB)               │
               │ - Kernel heap (kmalloc)         │
               │ - Page tables                   │
               │ - Process memory                │
               │ - DMA buffers                   │
               │                                 │
   0x87FFFFFF  └─────────────────────────────────┘

Kernel sections are detailed in the "Kernel Sections" breakdown below.

Virtual Memory
--------------

Address Space Layout
~~~~~~~~~~~~~~~~~~~~

With Sv39 paging enabled, ThunderOS uses the following virtual address layout:

**Kernel Space (0x80000000 - 0xFFFFFFFF):**

.. code-block:: text

   0xFFFFFFFF  ┌─────────────────────────────────┐
               │ Reserved (future higher-half)   │
   0x80000000  ├─────────────────────────────────┤ ← KERNEL_VIRT_BASE
               │ Kernel Memory                   │
               │ - Kernel code/data              │
               │ - Page tables                   │
               │ - Kernel heap                   │
               │ - MMIO mappings                 │
               │ (identity mapped to physical)   │
               └─────────────────────────────────┘

**User Space (0x00000000 - 0x7FFFFFFF):**

.. code-block:: text

   0x7FFFFFFF  ┌─────────────────────────────────┐
   0x7FFFF000  ├─────────────────────────────────┤ ← USER_STACK_TOP
               │ Stack (1MB)                     │
               │ (grows downward)                │
   0x7FEFF000  ├─────────────────────────────────┤
               │ Unmapped (safety margin)        │
   0x40000000  ├─────────────────────────────────┤ ← USER_MMAP_START
               │ mmap region                     │
               │ (dynamically allocated)         │
               ├─────────────────────────────────┤
               │ Unmapped                        │
   0x10000000  ├─────────────────────────────────┤ ← USER_HEAP_BASE
               │ Heap (grows upward via brk)     │
               ├─────────────────────────────────┤
               │ BSS (zero-initialized)          │
               ├─────────────────────────────────┤
               │ Data (initialized variables)    │
               ├─────────────────────────────────┤
               │ Text (code)                     │
   0x00400000  ├─────────────────────────────────┤ ← USER_CODE_BASE
               │ Unmapped (null pointer guard)   │
   0x00000000  └─────────────────────────────────┘

**Key Boundaries:**

.. code-block:: c

   #define USER_CODE_BASE      0x00400000   // Code segment base
   #define USER_HEAP_BASE      0x10000000   // Heap start
   #define USER_MMAP_START     0x40000000   // mmap region
   #define USER_STACK_TOP      0x7FFFF000   // Stack top
   #define USER_STACK_SIZE     0x00100000   // 1MB stack
   #define KERNEL_VIRT_BASE    0x80000000   // Kernel boundary

Memory Isolation
----------------

ThunderOS provides complete per-process memory isolation to prevent processes from accessing each other's memory or corrupting kernel data.

Isolation Components
~~~~~~~~~~~~~~~~~~~~

1. **Per-Process Page Tables**
   
   - Each process has its own page table
   - Separate virtual address spaces
   - Kernel mappings shared (VPN[2] >= 2)
   - User mappings isolated (VPN[2] = 0-1)

2. **Virtual Memory Areas (VMAs)**
   
   - Track memory regions and permissions
   - Enforce read/write/execute access
   - Enable fine-grained control

3. **Isolated Heaps**
   
   - Each process has separate heap
   - Safety margins prevent collisions
   - Controlled via ``sys_brk()``

4. **Pointer Validation**
   
   - All user pointers validated before kernel access
   - Prevents kernel memory access from user mode
   - Checks VMA permissions

Virtual Memory Areas (VMAs)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VMAs track memory regions within a process:

.. code-block:: c

   typedef struct vm_area {
       uint64_t start;        // Start address (inclusive)
       uint64_t end;          // End address (exclusive)
       uint32_t flags;        // Protection flags
       struct vm_area *next;  // Linked list
   } vm_area_t;

**VMA Flags:**

- ``VM_READ (0x01)`` - Region is readable
- ``VM_WRITE (0x02)`` - Region is writable
- ``VM_EXEC (0x04)`` - Region is executable
- ``VM_USER (0x08)`` - Region is user-accessible
- ``VM_SHARED (0x10)`` - Shared between processes
- ``VM_GROWSDOWN (0x20)`` - Grows downward (stack)

**Example VMA Layout:**

.. code-block:: text

   Process VMA List:
   
   ┌─────────────────────────────────────────────┐
   │ VMA: 0x00400000-0x00500000                  │
   │ Flags: VM_READ | VM_EXEC | VM_USER          │
   │ (Code segment)                              │
   ├─────────────────────────────────────────────┤
   │ VMA: 0x10000000-0x10100000                  │
   │ Flags: VM_READ | VM_WRITE | VM_USER         │
   │ (Heap)                                      │
   ├─────────────────────────────────────────────┤
   │ VMA: 0x7FEFF000-0x7FFFF000                  │
   │ Flags: VM_READ | VM_WRITE | VM_USER         │
   │       | VM_GROWSDOWN                        │
   │ (Stack)                                     │
   └─────────────────────────────────────────────┘

Process Structure
~~~~~~~~~~~~~~~~~

Each process contains memory isolation fields:

.. code-block:: c

   struct process {
       // ... existing fields ...
       
       // Memory isolation
       page_table_t *page_table;    // Isolated page table
       vm_area_t *vm_areas;          // VMA list head
       uint64_t heap_start;          // Heap start
       uint64_t heap_end;            // Current heap end
   };

Page Table Management
~~~~~~~~~~~~~~~~~~~~~

**Creating User Page Tables:**

``create_user_page_table()`` creates an isolated page table:

1. Allocates root page table
2. Copies kernel entries (VPN[2] = 2-511) **by reference**
3. Zeroes user entries (VPN[2] = 0-1)
4. Maps UART/CLINT MMIO for kernel mode

**Important:** Kernel entries are shared, not duplicated!

.. warning::
   User page tables share kernel page table structures.
   Never free entries VPN[2] >= 2 when cleaning up!

**Freeing User Page Tables:**

``free_page_table()`` carefully frees only user-owned structures:

.. code-block:: c

   static void free_page_table_recursive(page_table_t *pt, int level) {
       if (level > 0) {
           // At level 2, only free user space (VPN[2] = 0-1)
           // Skip kernel space (VPN[2] = 2-511) - shared!
           int end_index = (level == 2) ? 2 : PT_ENTRIES;
           
           for (int i = 0; i < end_index; i++) {
               // Free child page tables
           }
       }
       pmm_free_page((uintptr_t)pt);
   }

**The Critical Bug:**

Initially, ``free_page_table_recursive()`` freed all entries (0-511), causing:

1. User page table freed, including shared kernel entries
2. Kernel page table structures freed (dangling pointers!)
3. Next allocation returned freed pages
4. Writing to "allocated" pages → page faults
5. System crashed with "Store/AMO page fault"

**The Fix:** Only free user space entries (VPN[2] = 0-1) at top level.

Key Functions
-------------

VMA Management
~~~~~~~~~~~~~~

``process_add_vma(proc, start, end, flags)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Adds a VMA to a process:

.. code-block:: c

   // Add executable code region
   process_add_vma(proc, 0x400000, 0x500000, 
                   VM_READ | VM_EXEC | VM_USER);

``process_find_vma(proc, addr)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Finds VMA containing an address:

.. code-block:: c

   vm_area_t *vma = process_find_vma(proc, 0x400100);
   if (vma && (vma->flags & VM_EXEC)) {
       // Address is in executable region
   }

``process_remove_vma(proc, vma)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Removes a VMA (does not unmap pages):

.. code-block:: c

   process_remove_vma(proc, vma);
   // Caller must also call unmap_page() separately

Memory Mapping
~~~~~~~~~~~~~~

``process_map_region(proc, vaddr, size, flags)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maps memory region:

.. code-block:: c

   // Map 8KB read-write region
   process_map_region(proc, 0x40000000, 8192,
                      VM_READ | VM_WRITE | VM_USER);

Implementation:
1. Validates alignment
2. Allocates physical pages
3. Maps in page table
4. Creates VMA

Pointer Validation
~~~~~~~~~~~~~~~~~~

``process_validate_user_ptr(proc, ptr, size, required_flags)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Validates user pointer before kernel access:

.. code-block:: c

   // Before reading from user buffer
   if (!process_validate_user_ptr(proc, buf, count, VM_READ)) {
       set_errno(THUNDEROS_EFAULT);
       return -1;
   }
   // Safe to access now

Checks:
1. Pointer not NULL
2. Address in user space (< 0x80000000)
3. No address wraparound
4. VMA exists for entire range
5. VMA has required permissions

System Calls
------------

Memory-Related Syscalls
~~~~~~~~~~~~~~~~~~~~~~~

``sys_brk(void *addr)``
^^^^^^^^^^^^^^^^^^^^^^^

Expands/shrinks process heap:

.. code-block:: c

   void *old_brk = sbrk(0);      // Get current
   void *new_brk = sbrk(4096);   // Expand by 4KB
   if (new_brk != (void*)-1) {
       // Successfully allocated
   }

Implementation:
- Validates addr >= heap_start
- Prevents heap-stack collision (maintains 2-page margin)
- Allocates/frees pages as needed
- Updates VMAs
- Returns new heap_end

``sys_mmap(void *addr, size_t len, int prot, int flags)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maps memory into address space:

.. code-block:: c

   void *mem = mmap(NULL, 8192, 
                    PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, 
                    -1, 0);

Implementation:
- Finds available address (if addr is NULL)
- Rounds length to pages
- Allocates physical pages
- Maps with requested protection
- Creates VMA

``sys_munmap(void *addr, size_t len)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Unmaps memory:

.. code-block:: c

   munmap(mem, 8192);

Implementation:
- Validates alignment
- Finds VMA
- Unmaps pages
- Frees physical pages
- Updates/removes VMA

Process Forking
---------------

``process_fork(struct process *parent)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Creates child process with memory copy:

**Memory Copying Strategy:**

1. **Page Table Duplication**
   
   - Allocates new page table for child
   - Copies kernel mappings (shared by reference)
   - Creates new user space mappings

2. **VMA Copying**
   
   - Walks parent's VMA list
   - Creates identical VMAs for child

3. **Physical Page Copying**
   
   - Allocates new physical pages
   - Copies content from parent
   - Maps in child's page table

4. **Heap Duplication**
   
   - Copies heap_start and heap_end
   - Allocates and copies heap pages

.. code-block:: c

   struct process *child = process_fork(current);
   if (child) {
       child->parent = current->pid;
       // Fork succeeded
   }

**Future:** Implement copy-on-write (COW) for efficiency.

Memory Safety
-------------

Safety Margins
~~~~~~~~~~~~~~

**Heap-Stack Margin:**

- Minimum 1MB gap enforced
- Checked in ``sys_brk()``
- Prevents heap overflow into stack

**Guard Pages:**

- Unmapped pages around stack
- Trigger page faults on overflow
- Help catch stack bugs

**Null Pointer Guard:**

- Region 0x00000000 - 0x003FFFFF unmapped
- Null pointer dereferences → page fault

Isolation Guarantees
~~~~~~~~~~~~~~~~~~~~

**What is Isolated:**

✓ User space memory (0x00000000 - 0x7FFFFFFF)
✓ Process heap and stack
✓ mmap regions
✓ Process-specific data

**What is Shared:**

✓ Kernel memory (0x80000000+) - for trap handling
✓ MMIO regions - for kernel operations
✓ Kernel page table structures - efficiency

Security Considerations
~~~~~~~~~~~~~~~~~~~~~~~

**Pointer Validation Required:**

.. code-block:: c

   // BAD - no validation
   strcpy(kernel_buf, user_ptr);
   
   // GOOD - validated
   if (!process_validate_user_ptr(proc, user_ptr, len, VM_READ)) {
       return -EFAULT;
   }
   memcpy(kernel_buf, user_ptr, len);

**TOCTOU Vulnerability:**

Current implementation vulnerable if user modifies memory between validation and use. Mitigation: copy to kernel buffer immediately after validation.

Error Handling
--------------

Error Codes
~~~~~~~~~~~

Memory functions use errno:

- ``THUNDEROS_EINVAL`` - Invalid parameter
- ``THUNDEROS_ENOMEM`` - Out of memory
- ``THUNDEROS_EFAULT`` - Invalid user pointer
- ``THUNDEROS_EACCES`` - Permission denied
- ``THUNDEROS_EEXIST`` - Mapping exists

Common Error Patterns
~~~~~~~~~~~~~~~~~~~~~

**VMA Not Found:**

.. code-block:: c

   vm_area_t *vma = process_find_vma(proc, addr);
   if (!vma) {
       set_errno(THUNDEROS_EFAULT);
       return -1;
   }

**Insufficient Permissions:**

.. code-block:: c

   if (!(vma->flags & VM_WRITE)) {
       set_errno(THUNDEROS_EACCES);
       return -1;
   }

**Heap Collision:**

.. code-block:: c

   if (new_heap_end > stack_bottom - MIN_GAP) {
       set_errno(THUNDEROS_ENOMEM);
       return current_heap_end;
   }

Testing
-------

Memory Isolation Tests
~~~~~~~~~~~~~~~~~~~~~~

Comprehensive test suite in ``tests/unit/test_memory_isolation.c`` with 15 tests:

1. Isolated page tables
2. VMA initialization
3. VMA permissions
4. Multiple VMAs
5. Heap boundaries
6. process_map_region
7. Pointer validation
8. Unmapped rejection
9. Kernel address rejection
10. VMA cleanup
11. Process isolation
12. VMA addition
13. VMA removal
14. Cross-process VMA isolation
15. Heap safety margins

**Running Tests:**

.. code-block:: bash

   make
   qemu-system-riscv64 -machine virt -m 128M -nographic \
       -serial mon:stdio -bios none -kernel build/thunderos.elf

**Expected Output:**

.. code-block:: text

   ========================================
     Memory Isolation Tests
   ========================================
   
   Test: Process has isolated page table... PASS
   Test: VMA list is initialized... PASS
   ...
   Test: Heap boundaries enforce safety margins... PASS
   
   ========================================
   Test Summary:
     Passed: 15 / 15
     Status: ALL TESTS PASSED!
   ========================================

Checking Memory Usage
~~~~~~~~~~~~~~~~~~~~~

**At Build Time:**

.. code-block:: bash

   riscv64-unknown-elf-size build/thunderos.elf

**At Runtime:**

.. code-block:: c

   extern char _kernel_end[];
   
   void print_memory_stats(void) {
       size_t free = 0x87000000 - (size_t)_kernel_end;
       uart_printf("Free RAM: %zu MB\n", free / 1024 / 1024);
   }

Known Limitations
-----------------

Current Limitations
~~~~~~~~~~~~~~~~~~~

1. **No Copy-on-Write**
   
   - ``fork()`` does full memory copy
   - Inefficient for large processes

2. **No Memory-Mapped Files**
   
   - ``mmap()`` only supports anonymous memory
   - Cannot map files

3. **No Shared Memory**
   
   - Processes cannot share memory
   - No shared memory IPC

4. **Fixed Address Layout**
   
   - No ASLR (Address Space Layout Randomization)
   - Security concern

5. **Linear VMA Search**
   
   - O(n) lookup time
   - Slow with many VMAs

Future Enhancements
~~~~~~~~~~~~~~~~~~~

**Planned:**

- Copy-on-write fork
- Memory-mapped files
- Shared memory regions
- ASLR support
- Swap support
- Memory limits and quotas
- OOM killer
- Huge pages (2MB/1GB)

See Also
--------

**Related Documentation:**

- :doc:`paging` - Page table implementation
- :doc:`pmm` - Physical memory allocator
- :doc:`kmalloc` - Kernel heap allocator
- :doc:`dma` - DMA memory allocator
- :doc:`process_management` - Process structures
- :doc:`syscalls` - System call interface
- :doc:`user_mode` - User mode execution
- :doc:`linker_script` - Memory layout definition

**Source Files:**

- ``kernel/core/process.c`` - Memory isolation implementation
- ``kernel/core/syscall.c`` - brk, mmap, munmap
- ``kernel/mm/paging.c`` - Page table operations
- ``kernel/mm/pmm.c`` - Physical memory manager
- ``kernel/mm/kmalloc.c`` - Kernel allocator
- ``include/kernel/process.h`` - Process and VMA structures
- ``tests/unit/test_memory_isolation.c`` - Test suite

**Key Functions:**

- ``process_setup_memory_isolation()``
- ``process_add_vma()``, ``process_find_vma()``, ``process_remove_vma()``
- ``process_map_region()``
- ``process_validate_user_ptr()``
- ``process_fork()``
- ``create_user_page_table()``, ``free_page_table()``
- ``sys_brk()``, ``sys_mmap()``, ``sys_munmap()``
