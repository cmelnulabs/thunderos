Physical Memory Manager (PMM)
==============================

Overview
--------

The Physical Memory Manager (PMM) is responsible for tracking and allocating physical memory at page granularity. It provides the low-level memory allocation service that higher-level allocators (like kmalloc) build upon.

**Key Features:**

* **Page-based allocation**: 4KB pages (RISC-V Sv39 standard)
* **Bitmap allocator**: Simple and efficient tracking
* **Linear search**: First-fit allocation strategy

**Current Limitations:**

* Maximum 32,768 pages (128MB) due to fixed bitmap size
* Single-page allocations only (no contiguous multi-page support yet)
* No NUMA awareness
* No page coloring or cache optimization

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
   
   Bitmap: 4096 bytes
   - Each bit represents one 4KB page
   - 0 = free, 1 = allocated
   - Supports 32,768 pages (128MB)

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

.. code-block:: c

   // Test if page is allocated
   static inline int bitmap_test(size_t page_num) {
       size_t byte_index = page_num / 8;
       size_t bit_index = page_num % 8;
       return (page_bitmap[byte_index] >> bit_index) & 1;
   }
   
   // Mark page as allocated
   static inline void bitmap_set(size_t page_num) {
       size_t byte_index = page_num / 8;
       size_t bit_index = page_num % 8;
       page_bitmap[byte_index] |= (1 << bit_index);
   }
   
   // Mark page as free
   static inline void bitmap_clear(size_t page_num) {
       size_t byte_index = page_num / 8;
       size_t bit_index = page_num % 8;
       page_bitmap[byte_index] &= ~(1 << bit_index);
   }

API Reference
-------------

Constants and Macros
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define PAGE_SIZE 4096                                  // 4KB pages
   #define PAGE_SHIFT 12                                   // log2(PAGE_SIZE)
   
   #define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
   #define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
   
   #define ADDR_TO_PAGE(addr) ((addr) >> PAGE_SHIFT)       // Physical address → page number
   #define PAGE_TO_ADDR(page) ((page) << PAGE_SHIFT)       // Page number → physical address

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

Multi-Page Allocation
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Allocate contiguous pages
   uintptr_t pmm_alloc_pages(size_t count);

**Use cases:** DMA buffers, large data structures, page tables

Free List Optimization
~~~~~~~~~~~~~~~~~~~~~~

Replace linear search with free list:

.. code-block:: text

   free_list_head → page_5 → page_12 → page_20 → NULL

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

**No Multi-Page Support**

Cannot allocate contiguous multi-page regions. Workaround:

.. code-block:: c

   // Allocate multiple single pages (not contiguous)
   for (int i = 0; i < 10; i++) {
       pages[i] = pmm_alloc_page();
   }

**No Fragmentation Tracking**

No statistics on fragmentation or largest contiguous block.

See Also
--------

* :doc:`kmalloc` - Kernel heap allocator (built on PMM)
* :doc:`memory_layout` - Physical memory layout
* :doc:`../riscv/memory_model` - RISC-V memory model
* :doc:`hal_timer` - Similar HAL abstraction pattern
