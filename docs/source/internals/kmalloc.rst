Kernel Heap Allocator (kmalloc)
================================

Overview
--------

The kernel heap allocator provides dynamic memory allocation for the kernel through ``kmalloc()`` and ``kfree()`` functions. It builds on top of the Physical Memory Manager (PMM) to provide variable-sized allocations.

**Key Features:**

* **Variable-size allocation**: Request any size, not just pages
* **Header-based tracking**: Metadata stored with each allocation
* **Magic number validation**: Detect memory corruption
* **Page-granular**: Currently allocates in 4KB chunks

**Current Limitations:**

* **Single-page limit**: Maximum allocation is ~4KB (PAGE_SIZE - HEADER_SIZE)
* **Internal fragmentation**: Small allocations waste page space
* **No slab allocator**: No size-class optimization
* **Identity mapping assumption**: Direct VA==PA, needs update for virtual memory

Design
------

Architecture
~~~~~~~~~~~~

kmalloc uses a header-based approach:

.. code-block:: text

   Allocated Block:
   
   ┌─────────────────────────┐ ← Physical page from PMM
   │  kmalloc_header         │
   │  ├── size: 256          │   24 bytes header
   │  ├── pages: 1           │
   │  └── magic: 0xDEADBEEF  │
   ├─────────────────────────┤ ← Returned pointer
   │                         │
   │  User Data (256 bytes)  │
   │                         │
   ├─────────────────────────┤
   │  Unused (~3816 bytes)   │   Internal fragmentation
   │                         │
   └─────────────────────────┘ ← End of 4KB page

Data Structures
~~~~~~~~~~~~~~~

.. code-block:: c

   struct kmalloc_header {
       size_t size;           // Requested size in bytes
       size_t pages;          // Number of pages allocated
       unsigned int magic;    // Validation: 0xDEADBEEF
   };
   
   #define KMALLOC_MAGIC 0xDEADBEEF
   #define HEADER_SIZE sizeof(struct kmalloc_header)  // 24 bytes on 64-bit

**Header Layout (64-bit RISC-V):**

.. code-block:: text

   Offset  Size  Field
   ------  ----  -----
   +0      8     size (size_t)
   +8      8     pages (size_t)
   +16     4     magic (unsigned int)
   +20     4     (padding for alignment)
   Total: 24 bytes

API Reference
-------------

Functions
~~~~~~~~~

**kmalloc(size)**

   Allocate kernel memory.
   
   :param size: Number of bytes to allocate
   :returns: Pointer to allocated memory, or NULL if out of memory
   
   **Behavior:**
   
   1. Add header size to requested size
   2. Calculate pages needed: ``(total_size + PAGE_SIZE - 1) / PAGE_SIZE``
   3. Check if multi-page (currently unsupported → return NULL)
   4. Allocate page via ``pmm_alloc_page()``
   5. Initialize header with size, pages, magic
   6. Return pointer after header
   
   **Example:**
   
   .. code-block:: c
   
      void *buffer = kmalloc(256);
      if (buffer == NULL) {
          hal_uart_puts("Out of memory\n");
          return;
      }
      
      // Use buffer
      char *str = (char *)buffer;
      str[0] = 'H';
      str[1] = 'i';
      
      kfree(buffer);

**kfree(ptr)**

   Free previously allocated kernel memory.
   
   :param ptr: Pointer returned by kmalloc (or NULL)
   :returns: void
   
   **Behavior:**
   
   1. Check for NULL (no-op if NULL)
   2. Calculate header address: ``ptr - HEADER_SIZE``
   3. Validate magic number (error if mismatch)
   4. Free all pages via ``pmm_free_page()``
   
   **Safety:**
   
   * Validates magic number to detect corruption
   * Prints error for invalid pointers
   * Tolerates NULL pointers (safe to call multiple times)
   
   **Example:**
   
   .. code-block:: c
   
      void *data = kmalloc(512);
      // ... use data ...
      kfree(data);
      kfree(data);  // Safe: NULL after first free (if we set it)

**kmalloc_aligned(size, align)**

   Allocate aligned kernel memory.
   
   :param size: Number of bytes to allocate
   :param align: Alignment requirement (must be power of 2)
   :returns: Pointer to aligned memory, or NULL if unsupported/OOM
   
   **Current Implementation:**
   
   * If align ≤ PAGE_SIZE (4KB): Use regular kmalloc (already page-aligned)
   * If align > PAGE_SIZE: Return NULL (not yet supported)
   
   **Future:** Support larger alignments via over-allocation
   
   **Example:**
   
   .. code-block:: c
   
      // Allocate 4KB-aligned buffer for DMA
      void *dma_buf = kmalloc_aligned(1024, 4096);

Implementation Details
----------------------

Allocation Algorithm
~~~~~~~~~~~~~~~~~~~~

File: ``kernel/mm/kmalloc.c``

.. code-block:: c

   void *kmalloc(size_t size) {
       if (size == 0) {
           return NULL;
       }
       
       // Include header in total size
       size_t total_size = size + HEADER_SIZE;
       
       // Calculate pages needed (round up)
       size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
       
       // Limitation: only single-page allocations
       if (pages_needed > 1) {
           hal_uart_puts("kmalloc: Multi-page allocation not yet supported\n");
           return NULL;
       }
       
       // Get physical page
       uintptr_t page_addr = pmm_alloc_page();
       if (page_addr == 0) {
           return NULL;  // Out of memory
       }
       
       // Initialize header
       // NOTE: Assumes identity mapping (VA == PA)
       struct kmalloc_header *header = (struct kmalloc_header *)page_addr;
       header->size = size;
       header->pages = pages_needed;
       header->magic = KMALLOC_MAGIC;
       
       // Return user pointer (after header)
       return (void *)(page_addr + HEADER_SIZE);
   }

**Identity Mapping Assumption:**

.. code-block:: c

   // Current: Direct cast works because VA == PA
   struct kmalloc_header *header = (struct kmalloc_header *)page_addr;
   
   // Future: Need VA->PA translation when paging enabled
   struct kmalloc_header *header = phys_to_virt(page_addr);

Deallocation Algorithm
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void kfree(void *ptr) {
       if (ptr == NULL) {
           return;  // Safe no-op
       }
       
       // Recover header from user pointer
       struct kmalloc_header *header = 
           (struct kmalloc_header *)((uintptr_t)ptr - HEADER_SIZE);
       
       // Validate magic number
       if (header->magic != KMALLOC_MAGIC) {
           hal_uart_puts("kfree: Invalid pointer or corrupted header\n");
           return;
       }
       
       // Free all allocated pages
       uintptr_t page_addr = (uintptr_t)header;
       for (size_t i = 0; i < header->pages; i++) {
           pmm_free_page(page_addr + (i * PAGE_SIZE));
       }
   }

**Error Detection:**

The magic number (``0xDEADBEEF``) helps detect:

* Double-free bugs
* Use-after-free (if page reused)
* Buffer underflow (corrupts header)
* Invalid pointers (random values won't match)

Size Calculation
~~~~~~~~~~~~~~~~

Maximum usable allocation per page:

.. code-block:: c

   PAGE_SIZE = 4096 bytes
   HEADER_SIZE = 24 bytes
   Max user data = 4096 - 24 = 4072 bytes

**Examples:**

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 40

   * - Request
     - Total
     - Pages
     - Result
   * - 100 bytes
     - 124 bytes
     - 1 page
     - ✅ Allocate
   * - 4000 bytes
     - 4024 bytes
     - 1 page
     - ✅ Allocate
   * - 4072 bytes
     - 4096 bytes
     - 1 page
     - ✅ Allocate (perfect fit)
   * - 4073 bytes
     - 4097 bytes
     - 2 pages
     - ❌ Multi-page not supported
   * - 8192 bytes
     - 8216 bytes
     - 3 pages
     - ❌ Multi-page not supported

Usage Example
-------------

File: ``kernel/main.c``

.. code-block:: c

   void kernel_main(void) {
       // Initialize subsystems
       hal_uart_init();
       trap_init();
       hal_timer_init(1000000);
       
       // Initialize memory management
       pmm_init(0x80200000, 126 * 1024 * 1024);
       
       // Allocate heap memory
       void *data = kmalloc(256);
       if (data == NULL) {
           hal_uart_puts("kmalloc failed\n");
           return;
       }
       
       hal_uart_puts("Allocated 256 bytes at: 0x");
       kprint_hex((uintptr_t)data);
       hal_uart_puts("\n");
       
       // Use the memory
       char *str = (char *)data;
       for (int i = 0; i < 256; i++) {
           str[i] = 'A' + (i % 26);
       }
       
       // Free when done
       kfree(data);
       hal_uart_puts("Memory freed\n");
       
       // Check stats
       size_t total, free;
       pmm_get_stats(&total, &free);
       hal_uart_puts("Free pages: ");
       kprint_dec(free);
       hal_uart_puts("\n");
   }

**Output:**

.. code-block:: text

   PMM initialized: 32248 pages (126 MB)
   Allocated 256 bytes at: 0x80200018
   Memory freed
   Free pages: 32248

Memory Layout
-------------

Allocated Block Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Physical Address: 0x80200000 (example)
   
   +0x00000 ┌────────────────────────┐
           │ size = 256             │  8 bytes
   +0x00008 ├────────────────────────┤
           │ pages = 1              │  8 bytes
   +0x00010 ├────────────────────────┤
           │ magic = 0xDEADBEEF     │  4 bytes
   +0x00014 ├────────────────────────┤
           │ (padding)              │  4 bytes
   +0x00018 ├────────────────────────┤ ← User pointer returned
           │                        │
           │   User Data (256 B)    │
           │                        │
   +0x00118 ├────────────────────────┤
           │                        │
           │  Unused (3816 bytes)   │
           │                        │
   +0x01000 └────────────────────────┘ ← End of page

Identity Mapping Example
~~~~~~~~~~~~~~~~~~~~~~~~~

Current (early boot without paging):

.. code-block:: text

   Physical Addr  Virtual Addr
   0x80200000  =  0x80200000  (identity mapping)
   
   ptr = (void *)page_addr;  // Works directly

Future (with virtual memory):

.. code-block:: text

   Physical Addr  Virtual Addr
   0x80200000  →  0xFFFFFFFF80200000  (kernel space)
   
   ptr = phys_to_virt(page_addr);  // Need translation

Future Improvements
-------------------

Multi-Page Allocation
~~~~~~~~~~~~~~~~~~~~~

Enable allocations larger than 4KB:

.. code-block:: c

   void *kmalloc(size_t size) {
       // ...
       
       // Allocate contiguous pages
       uintptr_t page_addr = pmm_alloc_pages(pages_needed);
       
       // ...
   }

**Requires:** PMM support for contiguous multi-page allocation.

Slab Allocator
~~~~~~~~~~~~~~

Optimize for common small sizes:

.. code-block:: text

   Slab Caches:
   - 16-byte objects
   - 32-byte objects
   - 64-byte objects
   - 128-byte objects
   - 256-byte objects
   - 512-byte objects
   - 1024-byte objects
   - Fallback to page allocator for larger

**Benefits:**

* Reduces fragmentation
* Improves cache locality
* Faster allocation for common sizes

Virtual Memory Support
~~~~~~~~~~~~~~~~~~~~~~

Update for paging:

.. code-block:: c

   void *kmalloc(size_t size) {
       uintptr_t phys_addr = pmm_alloc_page();
       void *virt_addr = phys_to_virt(phys_addr);
       
       struct kmalloc_header *header = (struct kmalloc_header *)virt_addr;
       // ...
       return virt_addr + HEADER_SIZE;
   }
   
   void kfree(void *virt_ptr) {
       struct kmalloc_header *header = virt_ptr - HEADER_SIZE;
       uintptr_t phys_addr = virt_to_phys((uintptr_t)header);
       pmm_free_page(phys_addr);
   }

Debug Features
~~~~~~~~~~~~~~

.. code-block:: c

   // Poison freed memory
   memset(ptr, 0xDD, size);  // Detect use-after-free
   
   // Allocation tracking
   struct alloc_info {
       void *ptr;
       size_t size;
       const char *file;
       int line;
   };

**Usage:**

.. code-block:: c

   #define kmalloc(size) __kmalloc(size, __FILE__, __LINE__)

Known Issues
------------

**Single-Page Limit**

Cannot allocate more than ~4KB. Workaround:

.. code-block:: c

   // Instead of kmalloc(8192):
   void *pages[2];
   pages[0] = (void *)pmm_alloc_page();
   pages[1] = (void *)pmm_alloc_page();
   // Not contiguous!

**High Fragmentation**

Small allocations waste most of the page:

.. code-block:: text

   kmalloc(10) uses 4096 bytes for 10 bytes → 99.7% waste

**No Alignment Control**

Cannot request alignments > 4KB.

**Identity Mapping Assumption**

Code assumes VA == PA. Comments note this:

.. code-block:: c

   // NOTE: Direct cast assumes identity mapping (VA == PA).
   // Will need VA->PA translation when paging is enabled.

**No Zero-Initialization**

Returned memory contains previous data. For security:

.. code-block:: c

   void *buffer = kmalloc(size);
   memset(buffer, 0, size);  // Manual zeroing

See Also
--------

* :doc:`pmm` - Physical Memory Manager (underlying allocator)
* :doc:`memory_layout` - Physical memory layout
* :doc:`kstring` - Helper functions using kmalloc
* :doc:`../riscv/memory_model` - RISC-V virtual memory
