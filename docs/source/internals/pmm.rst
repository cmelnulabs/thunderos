Physical Memory Manager (PMM)
==============================

Overview
--------

The Physical Memory Manager (PMM) is responsible for tracking and allocating physical memory at page granularity. It provides the low-level memory allocation service that higher-level allocators (like kmalloc) build upon.

**Key Features:**

* **Page-based allocation**: 4KB pages (RISC-V Sv39 standard)
* **Bitmap allocator**: Simple and efficient tracking
* **Linear search**: First-fit allocation strategy
* **Multi-page allocation**: Allocate contiguous page ranges
* **Comprehensive validation**: Address alignment and range checking

**Current Limitations:**

* Maximum 32,768 pages (128MB) due to fixed bitmap size
* No NUMA awareness
* No page coloring or cache optimization
* No free list optimization (linear search can be slow)

Design
------

Architecture
~~~~~~~~~~~~

The PMM uses a bitmap to track page allocation state:

.. code-block:: text

   Physical Memory Layout:
   
   0x80000000 ┌────────────────┐
              │ Kernel Image   │ (Reserved - not managed by PMM)
              ├────────────────┤
   mem_start  │                │ ← PMM manages from here
              │                │
              │  Free Pages    │   Bitmap: [0 0 0 0 0 ...]
              │                │            ↑ ↑ ↑ ↑ ↑
              │                │            │ │ │ │ └─ Page 4: Free
              ├────────────────┤            │ │ │ └─── Page 3: Free
              │ Allocated Page │            │ │ └───── Page 2: Free
              ├────────────────┤            │ └─────── Page 1: Free
              │  Free Pages    │            └───────── Page 0: Free
              │                │
   0x88000000 └────────────────┘ (128MB total on QEMU)
   
   Bitmap: 4096 bytes = 32,768 bits
   - Each bit represents one 4KB page
   - 0 = free, 1 = allocated
   - Calculation: 4096 bytes × 8 bits/byte = 32,768 bits = 32,768 pages
   - Maximum memory tracked: 32,768 pages × 4KB/page = 128MB

Data Structures
~~~~~~~~~~~~~~~

.. code-block:: c

   // Memory region tracking
   static uintptr_t memory_start;    // First managed page address
   static size_t total_pages;        // Total pages managed
   static size_t free_pages;         // Currently free pages
   
   // Bitmap: 4096 bytes supports 32,768 pages (128MB)
   #define BITMAP_SIZE 4096
   static uint8_t page_bitmap[BITMAP_SIZE];

Bitmap Operations
~~~~~~~~~~~~~~~~~

The bitmap uses bit-level operations to efficiently track page allocation status. Each bit represents one 4KB page.

**Test if page is allocated:**

.. code-block:: c

   static inline int bitmap_test(size_t page_num) {
       size_t byte_index = page_num / 8;        // Which byte contains this page's bit
       size_t bit_index = page_num % 8;         // Which bit within that byte (0-7)
       return (page_bitmap[byte_index] >> bit_index) & 1;
   }

**How it works:**

1. **Find the byte**: Divide page number by 8 (since 8 bits per byte)
   
   Example: Page 25 → ``25 / 8 = 3`` (byte 3)

2. **Find the bit**: Remainder tells us which bit (0-7)
   
   Example: Page 25 → ``25 % 8 = 1`` (bit 1)

3. **Right shift**: Move the desired bit to position 0
   
   Example: ``page_bitmap[3] = 0b10110110`` → shift right 1 → ``0b01011011``

4. **Mask with 1**: Extract only the rightmost bit
   
   Example: ``0b01011011 & 0b00000001 = 1`` (page is allocated)

**Visual example:**

.. code-block:: text

   Bitmap byte 3: [0 1 1 0 1 1 1 0]
   Bit positions:  7 6 5 4 3 2 1 0
   
   Page 25 = byte 3, bit 1
   Bit 1 = 1 → Page is allocated

**Mark page as allocated:**

.. code-block:: c

   static inline void bitmap_set(size_t page_num) {
       size_t byte_index = page_num / 8;
       size_t bit_index = page_num % 8;
       page_bitmap[byte_index] |= (1 << bit_index);
   }

**How it works:**

1. **Find byte and bit**: Same as bitmap_test (byte 3, bit 1 for page 25)

2. **Create bit mask**: Left shift 1 by bit position
   
   Example: ``1 << 1 = 0b00000010`` (bit 1 set)

3. **OR operation**: Set the bit without changing others
   
   Example: ``0b10110110 | 0b00000010 = 0b10110110`` (bit already set)
   
   Or: ``0b10110100 | 0b00000010 = 0b10110110`` (bit now set)

**Visual example:**

.. code-block:: text

   Before: [0 1 1 0 1 1 0 0]  (Page 25 free, bit 1 = 0)
                       ↓
   Mask:   [0 0 0 0 0 0 1 0]  (1 << 1)
                       ↓
   OR:     [0 1 1 0 1 1 1 0]  (Page 25 now allocated, bit 1 = 1)

**Mark page as free:**

.. code-block:: c

   static inline void bitmap_clear(size_t page_num) {
       size_t byte_index = page_num / 8;
       size_t bit_index = page_num % 8;
       page_bitmap[byte_index] &= ~(1 << bit_index);
   }

**How it works:**

1. **Find byte and bit**: Same as before

2. **Create bit mask**: ``1 << bit_index`` creates mask with bit set
   
   Example: ``1 << 1 = 0b00000010``

3. **Invert mask**: ``~`` flips all bits (NOT operation)
   
   Example: ``~0b00000010 = 0b11111101``

4. **AND operation**: Clear the bit without changing others
   
   Example: ``0b10110110 & 0b11111101 = 0b10110100`` (bit 1 cleared)

**Visual example:**

.. code-block:: text

   Before: [0 1 1 0 1 1 1 0]  (Page 25 allocated, bit 1 = 1)
                       ↓
   Mask:   [0 0 0 0 0 0 1 0]  (1 << 1)
                       ↓
   ~Mask:  [1 1 1 1 1 1 0 1]  (~mask = inverted)
                       ↓
   AND:    [0 1 1 0 1 1 0 0]  (Page 25 now free, bit 1 = 0)

**Why use bitwise operations?**

* **Space efficient**: 1 bit per page vs 1 byte per page (8x space saving)
* **Fast**: Bitwise operations are single CPU instructions
* **Cache friendly**: More pages fit in cache lines

**Example: Mapping pages to bits:**

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 40

   * - Page Number
     - Byte Index
     - Bit Index
     - Calculation
   * - 0
     - 0
     - 0
     - ``0/8=0, 0%8=0``
   * - 7
     - 0
     - 7
     - ``7/8=0, 7%8=7``
   * - 8
     - 1
     - 0
     - ``8/8=1, 8%8=0``
   * - 25
     - 3
     - 1
     - ``25/8=3, 25%8=1``
   * - 32767
     - 4095
     - 7
     - ``32767/8=4095, 32767%8=7``

API Reference
-------------

Constants and Macros
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define PAGE_SIZE 4096                                  // 4KB pages
   #define PAGE_SHIFT 12                                   // log\ :sub:`2`\ (PAGE_SIZE) = log\ :sub:`2`\ (4096) = 12
   
   #define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
   #define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
   
   #define ADDR_TO_PAGE(addr) ((addr) >> PAGE_SHIFT)       // Physical address → page number
   #define PAGE_TO_ADDR(page) ((page) << PAGE_SHIFT)       // Page number → physical address

**PAGE_ALIGN_DOWN Explanation:**

Rounds an address **down** to the nearest page boundary (previous 4KB boundary).

**How it works:**

.. code-block:: c

   PAGE_SIZE - 1 = 4096 - 1 = 4095 = 0x0FFF = 0b111111111111
   
   ~(PAGE_SIZE - 1) = ~0x0FFF = 0xFFFFFFFFFFFFF000
   
   Result: addr & 0xFFFFFFFFFFFFF000

The mask ``~(PAGE_SIZE - 1)`` has the **lower 12 bits cleared** and all upper bits set. ANDing with this mask **clears the lower 12 bits** of the address, effectively rounding down to the page boundary.

**Example:**

.. code-block:: text

   Address: 0x80208ABC (somewhere in the middle of a page)
   
   Binary breakdown:
   0x80208ABC = 0b...10000000001000001000101010111100
   
   Mask = ~(4095) = 0b...11111111111111111111000000000000
                                            └──── 12 zeros
   
   Result:
   0x80208ABC & 0xFFFFF000 = 0x80208000 (rounded down to page start)
   
   Visual:
   0x80208ABC  →  0x80208000
   (within page)  (page boundary)

**More examples:**

.. list-table::
   :header-rows: 1
   :widths: 40 40 20

   * - Input Address
     - PAGE_ALIGN_DOWN Result
     - Offset Removed
   * - ``0x80200000``
     - ``0x80200000``
     - Already aligned
   * - ``0x80200001``
     - ``0x80200000``
     - 1 byte
   * - ``0x80200FFF``
     - ``0x80200000``
     - 4095 bytes
   * - ``0x80201000``
     - ``0x80201000``
     - Already aligned
   * - ``0x80208ABC``
     - ``0x80208000``
     - 2748 bytes

**PAGE_ALIGN_UP Explanation:**

Rounds an address **up** to the next page boundary (next 4KB boundary).

**How it works:**

.. code-block:: c

   Step 1: addr + PAGE_SIZE - 1
   Step 2: Result & ~(PAGE_SIZE - 1)

**Why add PAGE_SIZE - 1?**

This ensures that if the address is already aligned, it stays the same. If not aligned, it pushes into the next page before the mask rounds down.

**Example:**

.. code-block:: text

   Address: 0x80208ABC (somewhere in the middle of a page)
   
   Step 1: Add 4095
   0x80208ABC + 0x0FFF = 0x80209ABB
   
   Step 2: Round down (same as PAGE_ALIGN_DOWN)
   0x80209ABB & 0xFFFFF000 = 0x80209000
   
   Result: 0x80209000 (next page boundary)
   
   Visual:
   0x80208ABC  →  0x80209000
   (within page)  (next page boundary)

**More examples:**

.. list-table::
   :header-rows: 1
   :widths: 40 40 20

   * - Input Address
     - PAGE_ALIGN_UP Result
     - Notes
   * - ``0x80200000``
     - ``0x80200000``
     - Already aligned, stays same
   * - ``0x80200001``
     - ``0x80201000``
     - Rounds up to next page
   * - ``0x80200FFF``
     - ``0x80201000``
     - Rounds up to next page
   * - ``0x80201000``
     - ``0x80201000``
     - Already aligned, stays same
   * - ``0x80208ABC``
     - ``0x80209000``
     - Rounds up to next page

**Detailed walkthrough for PAGE_ALIGN_UP(0x80200001):**

.. code-block:: text

   Input: 0x80200001
   
   Step 1: Add (PAGE_SIZE - 1) = Add 0x0FFF
   0x80200001 + 0x0FFF = 0x80201000
   
   Step 2: Apply mask ~(PAGE_SIZE - 1) = 0xFFFFF000
   0x80201000 & 0xFFFFF000 = 0x80201000
   
   Result: 0x80201000 (next page)

**Detailed walkthrough for PAGE_ALIGN_UP(0x80200000):**

.. code-block:: text

   Input: 0x80200000 (already aligned)
   
   Step 1: Add (PAGE_SIZE - 1) = Add 0x0FFF
   0x80200000 + 0x0FFF = 0x80200FFF
   
   Step 2: Apply mask ~(PAGE_SIZE - 1) = 0xFFFFF000
   0x80200FFF & 0xFFFFF000 = 0x80200000
   
   Result: 0x80200000 (stays same because already aligned)

**Visual comparison:**

.. code-block:: text

   Memory layout:
   
   0x80208000 ┌─────────────┐  ← PAGE_ALIGN_DOWN(0x80208ABC)
              │   Page N    │
              │             │
   0x80208ABC │      •      │  ← Original address
              │             │
   0x80208FFF └─────────────┘
   0x80209000 ┌─────────────┐  ← PAGE_ALIGN_UP(0x80208ABC)
              │  Page N+1   │
              │             │
              │             │
   0x80209FFF └─────────────┘

**Common use cases:**

* **PAGE_ALIGN_DOWN**: Find the start of the page containing an address
* **PAGE_ALIGN_UP**: Allocate enough space to cover an address range

**Example usage in PMM:**

.. code-block:: c

   // Ensure memory_start begins at page boundary
   memory_start = PAGE_ALIGN_UP(mem_start);
   
   // Calculate which page contains an address
   uintptr_t page_start = PAGE_ALIGN_DOWN(some_address);

**PAGE_SHIFT Explanation:**

``PAGE_SHIFT`` is the **power of 2** that gives ``PAGE_SIZE``:

.. math::

   2^{12} = 4096 = \text{PAGE_SIZE}

This is used for efficient **bit shifting** instead of slow division/multiplication:

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - Operation
     - Slow Way
     - Fast Way (using PAGE_SHIFT)
   * - Address → Page number
     - ``addr / 4096``
     - ``addr >> 12``
   * - Page number → Address
     - ``page * 4096``
     - ``page << 12``

**Why bit shifting is faster:**

* Division/multiplication: ~20-40 CPU cycles
* Bit shift: **1 CPU cycle**

**Example conversions:**

.. code-block:: text

   Address 0x80208000 → Page number:
   
   Slow:  0x80208000 / 4096 = 0x80208 = 524,808
   Fast:  0x80208000 >> 12  = 0x80208 = 524,808
   
   Binary view:
   0x80208000 = 0b10000000001000001000000000000000
                                 ↑ Shift right 12 bits
                  0b100000000010000010000 = 0x80208
   
   ---
   
   Page 524,808 → Address:
   
   Slow:  524,808 * 4096 = 0x80208000
   Fast:  524,808 << 12  = 0x80208000
   
   Binary view:
   0x80208 = 0b100000000010000010000
                                     ↑ Shift left 12 bits (add 12 zeros)
             0b10000000001000001000000000000000 = 0x80208000

**Visual representation:**

.. code-block:: text

   Physical Address (64-bit):
   ┌──────────────────────────────────┬──────────────┐
   │     Page Number (52 bits)        │ Offset (12)  │
   └──────────────────────────────────┴──────────────┘
                                       ↑
                                    PAGE_SHIFT = 12
   
   Examples:
   0x80200000 = Page 524,800, Offset 0x000
   0x80200FFF = Page 524,800, Offset 0xFFF (last byte of page)
   0x80201000 = Page 524,801, Offset 0x000 (first byte of next page)

**Other PAGE_SHIFT values:**

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - Page Size
     - PAGE_SHIFT
     - Calculation
   * - 4 KB (4096)
     - 12
     - 2\ :sup:`12` = 4,096
   * - 2 MB
     - 21
     - 2\ :sup:`21` = 2,097,152
   * - 1 GB
     - 30
     - 2\ :sup:`30` = 1,073,741,824

Functions
~~~~~~~~~

**pmm_init(mem_start, mem_size)**

   Initialize the physical memory manager.
   
   :param mem_start: Start of usable physical memory (after kernel image)
   :param mem_size: Total size of usable memory in bytes
   :returns: void
   
   **Behavior:**
   
   * Aligns mem_start up to next page boundary
   * Calculates total number of pages
   * Clears bitmap (all pages initially free)
   * Prints initialization message with statistics
   
   **Example:**
   
   .. code-block:: c
   
      // Kernel ends at 0x80200000, total RAM is 128MB
      pmm_init(0x80200000, 126 * 1024 * 1024);
      // Output: "PMM initialized: 32248 pages (126 MB)"

**pmm_alloc_page()**

   Allocate a single physical page.
   
   :returns: Physical address of allocated page, or 0 if out of memory
   
   **Algorithm:**
   
   1. Linear search bitmap for first free page (bit = 0)
   2. Mark page as allocated (set bit = 1)
   3. Decrement free_pages counter
   4. Return physical address
   
   **Example:**
   
   .. code-block:: c
   
      uintptr_t page = pmm_alloc_page();
      if (page == 0) {
          hal_uart_puts("Out of memory!\n");
      } else {
          // Use page (e.g., page = 0x80208000)
      }

**pmm_alloc_pages(num_pages)**

   Allocate multiple contiguous physical pages.
   
   :param num_pages: Number of contiguous pages to allocate
   :returns: Physical address of first page, or 0 if cannot allocate
   
   **Algorithm:**
   
   1. Search bitmap for contiguous run of free pages
   2. Mark all pages in range as allocated
   3. Decrement free_pages counter by num_pages
   4. Return physical address of first page
   
   **Use cases:** DMA buffers, large data structures, page tables
   
   **Example:**
   
   .. code-block:: c
   
      // Allocate 16KB (4 contiguous pages)
      uintptr_t buffer = pmm_alloc_pages(4);
      if (buffer == 0) {
          hal_uart_puts("Cannot allocate contiguous pages!\n");
      } else {
          // Use buffer spanning 0x80200000 - 0x80203FFF
      }

**pmm_free_page(page_addr)**

   Free a previously allocated physical page.
   
   :param page_addr: Physical address of page to free (must be page-aligned)
   :returns: void
   
   **Validation:**
   
   * Checks if address is page-aligned
   * Checks if address is within managed memory range
   * Checks if page was actually allocated
   * Prints error if validation fails
   
   **Example:**
   
   .. code-block:: c
   
      pmm_free_page(0x80208000);  // Free the page

**pmm_free_pages(page_addr, num_pages)**

   Free multiple contiguous previously allocated physical pages.
   
   :param page_addr: Physical address of first page to free (must be page-aligned)
   :param num_pages: Number of contiguous pages to free
   :returns: void
   
   **Behavior:**
   
   * Frees each page individually with validation
   * All pages must be allocated before calling
   * Each page validated independently
   
   **Example:**
   
   .. code-block:: c
   
      // Free 4 contiguous pages starting at buffer
      pmm_free_pages(buffer, 4);

**pmm_get_stats(total_pages, free_pages)**

   Get memory usage statistics.
   
   :param total_pages: Output - total number of pages managed
   :param free_pages: Output - number of free pages available
   :returns: void
   
   **Example:**
   
   .. code-block:: c
   
      size_t total, free;
      pmm_get_stats(&total, &free);
      
      size_t used = total - free;
      size_t used_mb = (used * PAGE_SIZE) / (1024 * 1024);
      hal_uart_puts("Memory used: ");
      kprint_dec(used_mb);
      hal_uart_puts(" MB\n");

Implementation Details
----------------------

Initialization
~~~~~~~~~~~~~~

File: ``kernel/mm/pmm.c``

.. code-block:: c

   void pmm_init(uintptr_t mem_start, size_t mem_size) {
       // Align start to page boundary
       memory_start = PAGE_ALIGN_UP(mem_start);
       total_pages = mem_size / PAGE_SIZE;
       
       // Limit to bitmap capacity
       size_t max_pages = BITMAP_SIZE * 8;  // 32,768 pages
       if (total_pages > max_pages) {
           total_pages = max_pages;
       }
       
       free_pages = total_pages;
       
       // Clear bitmap (all pages free)
       for (size_t i = 0; i < BITMAP_SIZE; i++) {
           page_bitmap[i] = 0;
       }
       
       // Print status
       hal_uart_puts("PMM initialized: ");
       kprint_dec(total_pages);
       hal_uart_puts(" pages (");
       kprint_dec((total_pages * PAGE_SIZE) / (1024 * 1024));
       hal_uart_puts(" MB)\n");
   }

Allocation Algorithm
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   uintptr_t pmm_alloc_page(void) {
       // Check if any pages are available
       if (free_pages == 0) {
           return 0;  // Out of memory
       }
       
       // Linear search for first free page
       for (size_t i = 0; i < total_pages; i++) {
           if (!bitmap_test(i)) {
               // Found free page
               bitmap_set(i);
               free_pages--;
               
               // Calculate physical address
               uintptr_t page_addr = memory_start + (i * PAGE_SIZE);
               return page_addr;
           }
       }
       
       return 0;  // Should never reach here
   }

**Optimization Opportunities:**

* **Free list**: Maintain linked list of free pages for faster allocation
* **Binary search**: Use word-level operations to skip allocated regions
* **Buddy allocator**: Enable efficient multi-page allocations

Deallocation
~~~~~~~~~~~~

.. code-block:: c

   void pmm_free_page(uintptr_t page_addr) {
       // Validate alignment
       if (page_addr & (PAGE_SIZE - 1)) {
           hal_uart_puts("Error: page_addr not aligned\n");
           return;
       }
       
       // Validate range
       if (page_addr < memory_start) {
           hal_uart_puts("Error: page_addr before memory_start\n");
           return;
       }
       
       // Calculate page number
       size_t page_num = (page_addr - memory_start) / PAGE_SIZE;
       
       if (page_num >= total_pages) {
           hal_uart_puts("Error: page_num out of range\n");
           return;
       }
       
       // Validate page is allocated
       if (!bitmap_test(page_num)) {
           hal_uart_puts("Warning: freeing already-free page\n");
           return;
       }
       
       // Free the page
       bitmap_clear(page_num);
       free_pages++;
   }

Usage Example
-------------

File: ``kernel/main.c``

.. code-block:: c

   void kernel_main(void) {
       hal_uart_init();
       
       // Initialize PMM
       // Kernel occupies 0x80000000 - 0x80200000 (2MB)
       // Total RAM: 128MB (QEMU default)
       uintptr_t heap_start = 0x80200000;
       size_t heap_size = 126 * 1024 * 1024;  // 126MB
       
       pmm_init(heap_start, heap_size);
       
       // Allocate pages
       uintptr_t page1 = pmm_alloc_page();
       uintptr_t page2 = pmm_alloc_page();
       
       hal_uart_puts("Allocated page1: 0x");
       kprint_hex(page1);
       hal_uart_puts("\n");
       
       hal_uart_puts("Allocated page2: 0x");
       kprint_hex(page2);
       hal_uart_puts("\n");
       
       // Free a page
       pmm_free_page(page1);
       
       // Get statistics
       size_t total, free;
       pmm_get_stats(&total, &free);
       
       hal_uart_puts("Total pages: ");
       kprint_dec(total);
       hal_uart_puts(", Free: ");
       kprint_dec(free);
       hal_uart_puts("\n");
   }

**Output:**

.. code-block:: text

   PMM initialized: 32248 pages (126 MB)
   Allocated page1: 0x80200000
   Allocated page2: 0x80201000
   Total pages: 32248, Free: 32247

Memory Map
----------

QEMU virt Machine (128MB RAM)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Physical Address Space:
   
   0x00000000 - 0x00000FFF    Debug device
   0x00001000 - 0x0000FFFF    Reserved
   0x00010000 - 0x00010FFF    UART (NS16550A)
   0x02000000 - 0x0200FFFF    CLINT
   0x0C000000 - 0x0FFFFFFF    PLIC
   0x10000000 - 0x10000FFF    UART (legacy)
   
   0x80000000 - 0x801FFFFF    Kernel Image (2MB reserved)
                              ├── .text   (code)
                              ├── .rodata (read-only data)
                              ├── .data   (initialized data)
                              └── .bss    (uninitialized data)
   
   0x80200000 - 0x87FFFFFF    Free Memory (126MB)
                              Managed by PMM
                              32,248 pages of 4KB each
   
   0x88000000 - 0xFFFFFFFF    Not present (QEMU limit)

Page Alignment
~~~~~~~~~~~~~~

All allocations are page-aligned (4KB boundaries):

.. code-block:: text

   Page 0: 0x80200000 - 0x80200FFF
   Page 1: 0x80201000 - 0x80201FFF
   Page 2: 0x80202000 - 0x80202FFF
   ...
   Page 32247: 0x87FFF000 - 0x87FFFFFF

Future Improvements
-------------------

Free List Optimization
~~~~~~~~~~~~~~~~~~~~~~

Replace linear search with free list for faster allocation:

.. code-block:: text

   free_list_head → page_5 → page_12 → page_20 → NULL

**Benefits:**

* O(1) allocation instead of O(n) linear search
* Reduced cache misses
* Better performance under heavy allocation load

Buddy Allocator
~~~~~~~~~~~~~~~

Power-of-2 allocation sizes with efficient splitting/coalescing:

.. code-block:: text

   128KB block → split → 64KB + 64KB
                         ↓
                     32KB + 32KB + 64KB

NUMA Awareness
~~~~~~~~~~~~~~

Track pages by NUMA node for locality:

.. code-block:: c

   uintptr_t pmm_alloc_page_node(int node_id);

Page Coloring
~~~~~~~~~~~~~

Reduce cache conflicts by distributing pages across cache sets.

Known Issues
------------

**Bitmap Size Limitation**

Current bitmap is fixed at 4KB (32,768 pages = 128MB). For larger systems:

.. code-block:: c

   // TODO: Dynamic bitmap allocation
   // Allocate bitmap from first pages of managed memory

**Linear Search Performance**

Multi-page allocation searches linearly for contiguous blocks. For large allocations or fragmented memory, this can be slow:

* Single page: O(n) worst case
* Multi-page: O(n × m) where m = num_pages

Consider implementing free list or buddy allocator for better performance.

**No Fragmentation Tracking**

No statistics on fragmentation or largest contiguous block available.

See Also
--------

* :doc:`kmalloc` - Kernel heap allocator (built on PMM)
* :doc:`memory` - Complete memory management and layout
* :doc:`../riscv/memory_model` - RISC-V memory model
* :doc:`hal_timer` - Similar HAL abstraction pattern
