Virtual Memory (Paging)
=======================

ThunderOS implements virtual memory using the RISC-V **Sv39** paging scheme. This provides a 39-bit virtual address space with three-level page tables, enabling memory isolation, protection, and flexible memory management.

Overview
--------

Virtual memory separates the address space seen by programs (virtual addresses) from the actual physical memory addresses. This provides several benefits:

* **Memory Protection**: Each process can have its own isolated address space
* **Flexible Memory Layout**: Virtual addresses need not match physical addresses
* **Demand Paging**: Pages can be loaded on-demand (future feature)
* **Memory Sharing**: Multiple virtual pages can map to the same physical page

ThunderOS currently uses **identity mapping** (virtual address = physical address) for kernel space, with plans to move to a higher-half kernel mapping in the future.

RISC-V Sv39 Paging
------------------

Sv39 is a page-based virtual memory scheme for RISC-V 64-bit systems. Key characteristics:

* **39-bit Virtual Address Space**: 512 GB addressable memory
* **3-Level Page Tables**: Reduces memory overhead compared to flat tables
* **4 KB Page Size**: Standard page granularity
* **56-bit Physical Addresses**: Supports large physical memory

Virtual Address Layout
~~~~~~~~~~~~~~~~~~~~~~

A 39-bit virtual address is divided into:

.. code-block:: text

    63        39 38    30 29    21 20    12 11         0
    +-----------+--------+--------+--------+-------------+
    | Reserved  | VPN[2] | VPN[1] | VPN[0] |   Offset    |
    +-----------+--------+--------+--------+-------------+
         25         9        9        9          12

* **VPN[2], VPN[1], VPN[0]**: Virtual Page Numbers (9 bits each = 512 entries per level)
* **Offset**: Byte offset within the page (12 bits = 4 KB pages)
* **Reserved**: Must be sign-extended from bit 38

Page Table Entry (PTE)
~~~~~~~~~~~~~~~~~~~~~~

Each PTE is 64 bits:

.. code-block:: text

    63      54 53    28 27    19 18    10 9  8 7 6 5 4 3 2 1 0
    +----------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
    | Reserved |  PPN2  |  PPN1  |  PPN0  |RSW |D|A|G|U|X|W|R|V|
    +----------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
         10        26       9        9      2

**Permission Flags:**

* **V (Valid)**: Entry is valid (must be 1)
* **R (Read)**: Page is readable
* **W (Write)**: Page is writable
* **X (Execute)**: Page is executable
* **U (User)**: Page is accessible in user mode
* **G (Global)**: Mapping exists in all address spaces
* **A (Accessed)**: Page has been read/written/executed
* **D (Dirty)**: Page has been written

**Physical Page Number (PPN)**: Bits specifying the physical page address.

Page Table Walk
~~~~~~~~~~~~~~~

Address translation requires walking the page table hierarchy:

1. Start at root page table (from ``satp`` register)
2. Use VPN[2] to index into level 2 table → get PTE
3. Use VPN[1] to index into level 1 table → get PTE
4. Use VPN[0] to index into level 0 table → get PTE (leaf)
5. Extract PPN from leaf PTE
6. Combine PPN with offset to get physical address

If a PTE has R, W, or X bits set, it's a **leaf** (final mapping). Otherwise, it points to the next level table.

Data Structures
---------------

Page Table Entry
~~~~~~~~~~~~~~~~

.. code-block:: c

    typedef uint64_t pte_t;

Simple 64-bit integer representing a PTE.

Page Table
~~~~~~~~~~

.. code-block:: c

    typedef struct {
        pte_t entries[PT_ENTRIES];  // 512 entries
    } page_table_t;

A page table contains 512 PTEs (one 4 KB page).

Constants
~~~~~~~~~

.. code-block:: c

    #define PAGE_SIZE       4096      // 4 KB pages
    #define PT_ENTRIES      512       // Entries per page table
    
    // PTE flags
    #define PTE_V           0x001     // Valid
    #define PTE_R           0x002     // Readable
    #define PTE_W           0x004     // Writable
    #define PTE_X           0x008     // Executable
    #define PTE_U           0x010     // User accessible
    #define PTE_G           0x020     // Global
    #define PTE_A           0x040     // Accessed
    #define PTE_D           0x080     // Dirty
    
    // Common permission combinations
    #define PTE_KERNEL_DATA (PTE_R | PTE_W)
    #define PTE_KERNEL_CODE (PTE_R | PTE_X)
    #define PTE_USER_DATA   (PTE_R | PTE_W | PTE_U)
    #define PTE_USER_CODE   (PTE_R | PTE_X | PTE_U)

Virtual Memory Layout
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    // Kernel virtual base (higher-half kernel)
    #define KERNEL_VIRT_BASE    0xFFFFFFFF80000000UL
    
    // Physical memory base (QEMU virt machine)
    #define PHYSICAL_MEM_BASE   0x80000000

Currently, ThunderOS uses identity mapping where ``KERNEL_VIRT_BASE == PHYSICAL_MEM_BASE``.

API Reference
-------------

Initialization
~~~~~~~~~~~~~~

.. c:function:: void paging_init(uintptr_t kernel_start, uintptr_t kernel_end)

    Initialize the virtual memory system.
    
    :param kernel_start: Start address of kernel in memory
    :param kernel_end: End address of kernel in memory
    
    This function:
    
    * Creates the kernel page table
    * Identity maps the kernel code
    * Identity maps all RAM (for PMM/kmalloc)
    * Maps MMIO regions (UART, CLINT)
    * Enables Sv39 paging via the ``satp`` register
    
    **Called once during kernel initialization.**

Page Mapping
~~~~~~~~~~~~

.. c:function:: int map_page(page_table_t *page_table, uintptr_t vaddr, uintptr_t paddr, uint64_t flags)

    Map a virtual page to a physical page.
    
    :param page_table: Root page table
    :param vaddr: Virtual address (must be page-aligned)
    :param paddr: Physical address (must be page-aligned)
    :param flags: PTE permission flags (PTE_R, PTE_W, PTE_X, PTE_U, etc.)
    :return: 0 on success, -1 on failure
    
    **Implementation:**
    
    * Walks page table hierarchy, creating intermediate tables as needed
    * Creates leaf PTE with specified flags
    * Returns error if address already mapped
    
    **Example:**
    
    .. code-block:: c
    
        // Map a page for kernel data
        uintptr_t virt = 0x80200000;
        uintptr_t phys = pmm_alloc_page();
        map_page(pt, virt, phys, PTE_KERNEL_DATA);

.. c:function:: int unmap_page(page_table_t *page_table, uintptr_t vaddr)

    Unmap a virtual page.
    
    :param page_table: Root page table
    :param vaddr: Virtual address (must be page-aligned)
    :return: 0 on success, -1 on failure
    
    **Implementation:**
    
    * Walks page table to find PTE
    * Clears the PTE
    * Flushes TLB for the address
    * Does not free intermediate page tables (optimization for later)

Address Translation
~~~~~~~~~~~~~~~~~~~

.. c:function:: int virt_to_phys(page_table_t *page_table, uintptr_t vaddr, uintptr_t *paddr)

    Translate virtual address to physical address.
    
    :param page_table: Root page table
    :param vaddr: Virtual address
    :param paddr: Pointer to store physical address
    :return: 0 on success, -1 if not mapped
    
    Performs a software page table walk to translate addresses. Used when the MMU is disabled or for debugging.

.. c:function:: uintptr_t kernel_virt_to_phys(uintptr_t vaddr)

    Convert kernel virtual address to physical address.
    
    :param vaddr: Kernel virtual address
    :return: Physical address
    
    Simple inline function assuming identity mapping. Will be updated when higher-half kernel is implemented.

.. c:function:: uintptr_t kernel_phys_to_virt(uintptr_t paddr)

    Convert physical address to kernel virtual address.
    
    :param paddr: Physical address
    :return: Kernel virtual address

TLB Management
~~~~~~~~~~~~~~

.. c:function:: void tlb_flush(uintptr_t vaddr)

    Flush Translation Lookaside Buffer (TLB).
    
    :param vaddr: Virtual address to flush (0 = flush all)
    
    After modifying page tables, the TLB must be flushed so the CPU sees the changes. This executes the RISC-V ``sfence.vma`` instruction.

Page Table Access
~~~~~~~~~~~~~~~~~

.. c:function:: page_table_t *get_kernel_page_table(void)

    Get pointer to the kernel's root page table.
    
    :return: Pointer to kernel page table
    
    Useful for mapping kernel pages or when switching to user process page tables.

Macros
------

VPN Extraction
~~~~~~~~~~~~~~

.. code-block:: c

    #define VPN_0(va)   (((va) >> 12) & 0x1FF)  // Level 0 VPN
    #define VPN_1(va)   (((va) >> 21) & 0x1FF)  // Level 1 VPN
    #define VPN_2(va)   (((va) >> 30) & 0x1FF)  // Level 2 VPN

Extract Virtual Page Numbers from a virtual address.

PTE Manipulation
~~~~~~~~~~~~~~~~

.. code-block:: c

    #define PTE_TO_PA(pte)      (((pte) >> 10) << 12)
    #define PA_TO_PTE(pa, flags) ((((pa) >> 12) << 10) | (flags))

* ``PTE_TO_PA``: Extract physical address from PTE
* ``PA_TO_PTE``: Create PTE from physical address and flags

.. code-block:: c

    #define PTE_IS_LEAF(pte)    ((pte) & (PTE_R | PTE_W | PTE_X))

Check if PTE is a leaf (has R/W/X permissions).

SATP Register
~~~~~~~~~~~~~

.. code-block:: c

    #define SATP_MODE_SV39      (8UL << 60)
    #define SATP_PPN_SHIFT      12

* ``SATP_MODE_SV39``: Enable Sv39 paging mode
* ``SATP_PPN_SHIFT``: Shift for root page table PPN

The ``satp`` (Supervisor Address Translation and Protection) register controls paging:

.. code-block:: text

    63    60 59        44 43                    0
    +-------+-----------+----------------------+
    | MODE  |   ASID    |        PPN           |
    +-------+-----------+----------------------+

* **MODE**: Paging mode (8 = Sv39)
* **ASID**: Address Space ID (for TLB optimization)
* **PPN**: Physical page number of root page table

Current Implementation
----------------------

Identity Mapping
~~~~~~~~~~~~~~~~

ThunderOS currently uses **identity mapping** where virtual addresses equal physical addresses. This simplifies early kernel development:

.. code-block:: c

    virt_addr == phys_addr    // For all kernel pages

The entire 128 MB of RAM (0x80000000 - 0x88000000) is identity mapped.

MMIO Regions
~~~~~~~~~~~~

Memory-mapped I/O regions are also identity mapped:

* **UART**: 0x10000000 (for serial console)
* **CLINT**: 0x2000000 (for timer interrupts)

Page Table Structure
~~~~~~~~~~~~~~~~~~~~

Currently, ThunderOS has:

* **1 root page table** (statically allocated in ``kernel_page_table``)
* **Dynamic page tables** (allocated via ``pmm_alloc_page()`` as needed)

Limitations
~~~~~~~~~~~

* **No higher-half kernel**: Kernel not mapped to 0xFFFFFFFF80000000 yet
* **No user space**: All pages are kernel-accessible
* **No demand paging**: All pages mapped at initialization
* **No page table freeing**: Intermediate tables never freed when unmapping

Future Improvements
-------------------

Higher-Half Kernel
~~~~~~~~~~~~~~~~~~

Move kernel to 0xFFFFFFFF80000000:

* Update linker script (``kernel.ld``)
* Map kernel to both identity and higher-half during transition
* Switch to higher-half, unmap identity mapping
* Update all kernel code to use higher-half addresses

User Space Support
~~~~~~~~~~~~~~~~~~

Enable user processes:

* Per-process page tables
* User/kernel separation (use PTE_U flag)
* Context switching includes ``satp`` register swap

Demand Paging
~~~~~~~~~~~~~

Load pages on-demand:

* Handle page faults (trap handler)
* Allocate physical page on first access
* Support swapping to disk (far future)

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

* **Huge Pages**: 2 MB or 1 GB pages for large allocations
* **TLB Shootdown**: Coordinate TLB flushes across multiple CPUs
* **Page Table Caching**: Reuse freed page table pages

Memory Protection
~~~~~~~~~~~~~~~~~

* Enforce read-only kernel code (separate code/data mappings)
* Guard pages for stack overflow detection
* Execute-only pages for security

Usage Examples
--------------

Example 1: Map a Page
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "mm/paging.h"
    #include "mm/pmm.h"
    
    void example_map_page(void) {
        // Get kernel page table
        page_table_t *pt = get_kernel_page_table();
        
        // Allocate physical page
        uintptr_t phys = pmm_alloc_page();
        if (!phys) {
            return;  // Out of memory
        }
        
        // Map to virtual address
        uintptr_t virt = 0x90000000;  // Some virtual address
        int result = map_page(pt, virt, phys, PTE_KERNEL_DATA);
        
        if (result == 0) {
            // Success! Can now access memory at virt
            uint8_t *ptr = (uint8_t *)virt;
            *ptr = 42;
        }
    }

Example 2: Translate Address
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "mm/paging.h"
    
    void example_translate(void) {
        page_table_t *pt = get_kernel_page_table();
        
        uintptr_t virt = 0x80200000;  // Kernel address
        uintptr_t phys;
        
        if (virt_to_phys(pt, virt, &phys) == 0) {
            // Translation succeeded
            // For identity mapping: phys == virt
        }
    }

Example 3: Create User Page Table
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "mm/paging.h"
    #include "mm/pmm.h"
    
    page_table_t *create_user_page_table(void) {
        // Allocate page for root page table
        uintptr_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            return NULL;
        }
        
        page_table_t *pt = (page_table_t *)pt_phys;
        
        // Zero it out
        for (int i = 0; i < PT_ENTRIES; i++) {
            pt->entries[i] = 0;
        }
        
        // Map user code page
        uintptr_t code_phys = pmm_alloc_page();
        map_page(pt, 0x10000, code_phys, PTE_USER_CODE);
        
        // Map user data page
        uintptr_t data_phys = pmm_alloc_page();
        map_page(pt, 0x20000, data_phys, PTE_USER_DATA);
        
        return pt;
    }

Implementation Details
----------------------

Page Table Allocation
~~~~~~~~~~~~~~~~~~~~~

New page tables are allocated using the PMM:

.. code-block:: c

    static page_table_t *alloc_page_table(void) {
        uintptr_t page = pmm_alloc_page();
        if (page == 0) {
            return NULL;
        }
        
        // Zero out entries
        page_table_t *pt = (page_table_t *)page;
        for (int i = 0; i < PT_ENTRIES; i++) {
            pt->entries[i] = 0;
        }
        
        return pt;
    }

Page Table Walk
~~~~~~~~~~~~~~~

The ``walk_page_table()`` function traverses the 3-level hierarchy:

.. code-block:: c

    static pte_t *walk_page_table(page_table_t *root, 
                                    uintptr_t vaddr, 
                                    int create) {
        page_table_t *pt = root;
        
        // Walk levels 2 → 1 → 0
        for (int level = 2; level > 0; level--) {
            int vpn = extract_vpn(vaddr, level);
            pte_t *pte = &pt->entries[vpn];
            
            if (*pte & PTE_V) {
                // Entry exists, go to next level
                pt = (page_table_t *)PTE_TO_PA(*pte);
            } else {
                if (!create) return NULL;
                
                // Allocate new table
                pt = alloc_page_table();
                if (!pt) return NULL;
                
                // Link it
                *pte = PA_TO_PTE((uintptr_t)pt, PTE_V);
            }
        }
        
        // Return leaf PTE
        return &pt->entries[VPN_0(vaddr)];
    }

If ``create=1``, missing intermediate tables are allocated automatically.

Enabling Paging
~~~~~~~~~~~~~~~

Paging is enabled by setting the ``satp`` register:

.. code-block:: c

    static void enable_paging(page_table_t *root) {
        uintptr_t root_pa = (uintptr_t)root;
        uint64_t satp = SATP_MODE_SV39 | (root_pa >> SATP_PPN_SHIFT);
        
        asm volatile("csrw satp, %0" :: "r"(satp));
        
        // Flush TLB
        tlb_flush(0);
    }

After this, the CPU translates all addresses through the page table.

TLB Flush
~~~~~~~~~

The ``sfence.vma`` instruction flushes the TLB:

.. code-block:: c

    void tlb_flush(uintptr_t vaddr) {
        if (vaddr == 0) {
            // Flush all
            asm volatile("sfence.vma zero, zero" ::: "memory");
        } else {
            // Flush specific address
            asm volatile("sfence.vma %0, zero" :: "r"(vaddr) : "memory");
        }
    }

Always flush after modifying page tables!

Address Translation Helpers
~~~~~~~~~~~~~~~~~~~~~~~~~~~

ThunderOS v0.3.0 added convenience functions for address translation to support DMA operations and device drivers.

translate_virt_to_phys()
^^^^^^^^^^^^^^^^^^^^^^^^^

Convenience wrapper for virtual-to-physical translation using the kernel page table:

.. code-block:: c

    uintptr_t translate_virt_to_phys(uintptr_t vaddr) {
        uintptr_t paddr;
        
        // Try using page table translation
        if (virt_to_phys(&kernel_page_table, vaddr, &paddr) == 0) {
            return paddr;
        }
        
        // Fallback: assume identity mapping
        return vaddr;
    }

**Use case:** Device drivers need physical addresses for DMA but don't have direct page table access:

.. code-block:: c

    // Get physical address for DMA buffer
    uint8_t *buffer = kmalloc(4096);
    uintptr_t buffer_phys = translate_virt_to_phys((uintptr_t)buffer);
    
    // Program device DMA register
    device_reg_write(DMA_ADDR_LOW, buffer_phys & 0xFFFFFFFF);
    device_reg_write(DMA_ADDR_HIGH, buffer_phys >> 32);

**Behavior:**

* First attempts page table walk via ``virt_to_phys()``
* Returns physical address if mapping found
* Falls back to identity mapping assumption (current kernel design)
* Always succeeds for valid kernel addresses

translate_phys_to_virt()
^^^^^^^^^^^^^^^^^^^^^^^^^

Inline function for physical-to-virtual translation:

.. code-block:: c

    static inline uintptr_t translate_phys_to_virt(uintptr_t paddr) {
        // Currently using identity mapping for kernel
        return paddr;
    }

**Use case:** Converting physical addresses (from hardware) to virtual addresses for CPU access:

.. code-block:: c

    // Device gave us a physical address
    uintptr_t data_phys = device_reg_read(DATA_ADDR);
    
    // Convert to virtual for CPU access
    void *data_virt = (void *)translate_phys_to_virt(data_phys);
    
    // Now CPU can access the data
    process_data(data_virt, data_size);

**Current Implementation:**

* Simple identity mapping: physical = virtual
* Inline function (zero overhead)
* Will be updated when kernel moves to higher-half mapping

**Future (v0.4.0+):** When kernel uses higher-half mapping (virtual base 0xFFFFFFFF80000000):

.. code-block:: c

    static inline uintptr_t translate_phys_to_virt(uintptr_t paddr) {
        if (paddr < PHYS_MEMORY_SIZE) {
            return paddr + KERNEL_VIRT_BASE;
        }
        return paddr;  // MMIO regions stay identity-mapped
    }

kernel_virt_to_phys() / kernel_phys_to_virt()
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Inline helpers for kernel address conversion:

.. code-block:: c

    static inline uintptr_t kernel_virt_to_phys(uintptr_t vaddr) {
        if (vaddr >= KERNEL_VIRT_BASE) {
            return vaddr - KERNEL_VIRT_BASE;
        }
        // Identity mapped (early boot)
        return vaddr;
    }
    
    static inline uintptr_t kernel_phys_to_virt(uintptr_t paddr) {
        return paddr + KERNEL_VIRT_BASE;
    }

**Use case:** Explicit kernel address conversion (when higher-half is active):

.. code-block:: c

    // Convert kernel virtual to physical
    void *kernel_data = &some_kernel_global;
    uintptr_t phys = kernel_virt_to_phys((uintptr_t)kernel_data);
    
    // Convert physical to kernel virtual
    uintptr_t page_phys = pmm_alloc_page();
    void *page_virt = (void *)kernel_phys_to_virt(page_phys);

**Current Status:**

* Prepared for higher-half kernel
* Currently just returns identity mapping
* No overhead when not using higher-half

Integration with DMA Allocator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The DMA allocator (v0.3.0) uses these translation functions to track both virtual and physical addresses:

.. code-block:: c

    // DMA allocator allocates physical pages
    uintptr_t phys_addr = pmm_alloc_pages(num_pages);
    
    // Convert to virtual for CPU access (current: identity mapping)
    void *virt_addr = (void *)translate_phys_to_virt(phys_addr);
    
    // DMA region tracks both addresses
    region->phys_addr = phys_addr;  // For device
    region->virt_addr = virt_addr;  // For CPU

This abstraction allows DMA code to work unchanged when the kernel moves to higher-half mapping.

Testing
-------

See ``tests/test_paging.c`` for unit tests covering:

* Virtual-to-physical translation
* Page mapping and unmapping
* Kernel address helpers

References
----------

* `RISC-V Privileged Specification <https://riscv.org/specifications/privileged-isa/>`_
* `RISC-V Sv39 Virtual Memory <https://five-embeddev.com/riscv-isa-manual/latest/supervisor.html#sec:sv39>`_
* `xv6 RISC-V <https://github.com/mit-pdos/xv6-riscv>`_ - Educational OS with excellent paging implementation
