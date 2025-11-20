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

* **Internal fragmentation**: Small allocations waste page space
* **No slab allocator**: No size-class optimization
* **Identity mapping assumption**: Direct VA==PA, needs update for virtual memory
* **No free list**: Each allocation requires PMM search

Design
------

Architecture
~~~~~~~~~~~~

kmalloc uses a **header-based allocation strategy** where metadata is stored immediately before the user's data. This allows the allocator to track allocation size and validity without maintaining a separate data structure.

**How it works:**

1. **User requests N bytes** via ``kmalloc(N)``
2. **Allocator calculates total needed**: ``N + 24 bytes (header)``
3. **Rounds up to pages**: ``(total + 4095) / 4096`` pages
4. **Allocates pages from PMM** (contiguous physical pages)
5. **Writes header** at start of first page
6. **Returns pointer** to memory **after** the header

**Single-page allocation example:**

.. code-block:: text

   User calls: kmalloc(256)
   
   ┌─────────────────────────┐ ← 0x80200000 (from PMM)
   │  kmalloc_header         │
   │  ┌───────────────────┐  │
   │  │ size = 256        │  │   } 24 bytes
   │  │ pages = 1         │  │   } metadata
   │  │ magic = 0xDEADBEEF│  │   } (header)
   │  └───────────────────┘  │
   ├─────────────────────────┤ ← 0x80200018 (returned to user)
   │                         │
   │  User Data (256 bytes)  │   User writes here
   │  Available: bytes       │
   │  0-255                  │
   │                         │
   ├─────────────────────────┤ ← 0x80200118
   │                         │
   │  Unused (3816 bytes)    │   Internal fragmentation
   │  Wasted space           │   (not accessible)
   │                         │
   └─────────────────────────┘ ← 0x80201000 (end of 4KB page)
   
   Total allocated: 4096 bytes (1 page)
   User requested: 256 bytes
   Actually usable: 256 bytes
   Wasted: 3816 bytes (93.2% waste)

**Multi-page allocation example:**

.. code-block:: text

   User calls: kmalloc(10000)
   
   Total needed: 10000 + 24 = 10024 bytes
   Pages needed: (10024 + 4095) / 4096 = 3 pages = 12288 bytes
   
   ┌─────────────────────────┐ ← 0x80200000 (Page 0)
   │  kmalloc_header         │
   │  ┌───────────────────┐  │
   │  │ size = 10000      │  │   24 bytes header
   │  │ pages = 3         │  │
   │  │ magic = 0xDEADBEEF│  │
   │  └───────────────────┘  │
   ├─────────────────────────┤ ← 0x80200018 (returned to user)
   │                         │
   │  User Data              │
   │  (10000 bytes)          │   User writes bytes 0-9999
   │                         │
   │  Spans across:          │
   │  - Rest of page 0       │   (4096 - 24 = 4072 bytes)
   │  - All of page 1        │   (4096 bytes)
   │  - Part of page 2       │   (10000 - 4072 - 4096 = 1832 bytes)
   ├─────────────────────────┤ ← 0x80202728 (end of user data)
   │  Unused (2264 bytes)    │   Internal fragmentation
   └─────────────────────────┘ ← 0x80203000 (end of 3 pages)
   
   Total allocated: 12288 bytes (3 pages)
   User requested: 10000 bytes
   Actually usable: 10000 bytes
   Wasted: 2264 bytes (18.4% waste)

**Why use headers?**

* **No external tracking**: Header travels with the allocation
* **Fast deallocation**: ``kfree()`` finds size/pages from header
* **Corruption detection**: Magic number validates pointer
* **Simple implementation**: No need for free lists or bitmaps

**Trade-offs:**

* ✅ **Pros**: Simple, fast lookup, self-contained
* ❌ **Cons**: 24-byte overhead per allocation, high fragmentation for small allocations

Data Structures
~~~~~~~~~~~~~~~

.. code-block:: c

   struct kmalloc_header {
       size_t size;           // User-requested size in bytes (e.g., 256)
       size_t pages;          // Number of pages allocated (e.g., 1)
       unsigned int magic;    // Validation: 0xDEADBEEF (detect corruption)
   };
   
   #define KMALLOC_MAGIC 0xDEADBEEF
   #define HEADER_SIZE sizeof(struct kmalloc_header)  // 24 bytes on 64-bit

**Header Layout (64-bit RISC-V):**

.. code-block:: text

   Memory Address         Offset  Size  Field           Value Example
   ------------------     ------  ----  --------------  -------------
   0x80200000 (page start)  +0      8   size (size_t)   256
   0x80200008               +8      8   pages (size_t)  1
   0x80200010               +16     4   magic (uint)    0xDEADBEEF
   0x80200014               +20     4   (padding)       (unused)
   0x80200018 (user ptr)    +24     -   User data       (user writes here)
   
   Total header: 24 bytes

**Field explanations:**

* ``size``: Original user request (needed for debugging, not used in kfree)
* ``pages``: How many contiguous pages to free (critical for kfree)
* ``magic``: Constant ``0xDEADBEEF`` - if this changes, memory is corrupted
* ``padding``: Compiler adds 4 bytes to align struct to 8-byte boundary

**How kfree() uses the header:**

.. code-block:: c

   void kfree(void *user_ptr) {
       // Step 1: Go backwards 24 bytes to find header
       struct kmalloc_header *header = user_ptr - HEADER_SIZE;
       
       // Step 2: Validate magic (detect corruption/invalid pointer)
       if (header->magic != 0xDEADBEEF) {
           kernel_panic("Memory corruption detected!");
       }
       
       // Step 3: Read how many pages to free
       size_t pages_to_free = header->pages;  // e.g., 3
       
       // Step 4: Free all pages
       pmm_free_pages((uintptr_t)header, pages_to_free);
   }

**Visual: Finding the header:**

.. code-block:: text

   User has pointer: 0x80200018
   
   kfree(0x80200018)
     ↓
   header = 0x80200018 - 24 = 0x80200000
     ↓
   Read header at 0x80200000:
     size = 256
     pages = 1
     magic = 0xDEADBEEF ✓ Valid!
     ↓
   Free 1 page starting at 0x80200000

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
   3. If single page: call ``pmm_alloc_page()``
   4. If multiple pages: call ``pmm_alloc_pages(pages_needed)``
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
   3. Validate magic number (calls ``kernel_panic()`` if mismatch)
   4. If single page: call ``pmm_free_page()``
   5. If multiple pages: call ``pmm_free_pages(page_addr, pages)``
   
   **Safety:**
   
   * Validates magic number to detect corruption
   * Calls ``kernel_panic()`` for invalid pointers (system halt)
   * Tolerates NULL pointers (safe to call multiple times)
   * Handles both single-page and multi-page allocations
   
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

The allocation process has **five distinct steps**: validate request, calculate size, allocate pages, initialize header, and return pointer.

.. code-block:: c

   void *kmalloc(size_t size) {
       // Step 1: Validate request
       if (size == 0) {
           return NULL;  // Nothing to allocate
       }
       
       // Step 2: Calculate total size including header
       size_t total_size = size + HEADER_SIZE;
       
       // Step 3: Calculate pages needed (round up division)
       size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
       
       // Step 4: Allocate page(s) from PMM
       uintptr_t page_addr;
       if (pages_needed == 1) {
           page_addr = pmm_alloc_page();      // Single page fast path
       } else {
           page_addr = pmm_alloc_pages(pages_needed);  // Multi-page
       }
       
       if (page_addr == 0) {
           return NULL;  // Out of memory
       }
       
       // Step 5: Initialize header at start of allocation
       // NOTE: Assumes identity mapping (VA == PA)
       struct kmalloc_header *header = (struct kmalloc_header *)page_addr;
       header->size = size;
       header->pages = pages_needed;
       header->magic = KMALLOC_MAGIC;
       
       // Step 6: Return user pointer (skip over header)
       return (void *)(page_addr + HEADER_SIZE);
   }

**Step-by-step walkthrough for kmalloc(256):**

.. code-block:: text

   Step 1: Validate
   ---------------
   size = 256 bytes
   256 != 0, so continue
   
   Step 2: Calculate total size
   ----------------------------
   total_size = size + HEADER_SIZE
   total_size = 256 + 24 = 280 bytes
   
   Step 3: Calculate pages needed
   ------------------------------
   pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE
   pages_needed = (280 + 4095) / 4096
   pages_needed = 4375 / 4096
   pages_needed = 1 page
   
   Why add PAGE_SIZE - 1 before dividing?
   This is "round up" division:
   - If total_size = 4096: (4096 + 4095) / 4096 = 8191 / 4096 = 1 ✓
   - If total_size = 4097: (4097 + 4095) / 4096 = 8192 / 4096 = 2 ✓
   
   Step 4: Allocate pages
   ----------------------
   pages_needed == 1, so call pmm_alloc_page()
   PMM returns: page_addr = 0x80200000
   
   Step 5: Initialize header
   -------------------------
   Write to 0x80200000:
   [0x80200000] size   = 256         (8 bytes)
   [0x80200008] pages  = 1           (8 bytes)
   [0x80200010] magic  = 0xDEADBEEF  (4 bytes)
   [0x80200014] (padding)            (4 bytes)
   
   Step 6: Return user pointer
   ---------------------------
   user_ptr = page_addr + HEADER_SIZE
   user_ptr = 0x80200000 + 24
   user_ptr = 0x80200018
   
   Return: 0x80200018

**Step-by-step walkthrough for kmalloc(10000):**

.. code-block:: text

   Step 1: Validate
   ---------------
   size = 10000 bytes ✓
   
   Step 2: Calculate total size
   ----------------------------
   total_size = 10000 + 24 = 10024 bytes
   
   Step 3: Calculate pages needed
   ------------------------------
   pages_needed = (10024 + 4095) / 4096
   pages_needed = 14119 / 4096
   pages_needed = 3 pages
   
   Total space: 3 × 4096 = 12288 bytes
   
   Step 4: Allocate pages
   ----------------------
   pages_needed == 3, so call pmm_alloc_pages(3)
   PMM returns 3 contiguous pages: 0x80200000, 0x80201000, 0x80202000
   page_addr = 0x80200000
   
   Step 5: Initialize header
   -------------------------
   Write to 0x80200000:
   [0x80200000] size   = 10000       (8 bytes)
   [0x80200008] pages  = 3           (8 bytes)
   [0x80200010] magic  = 0xDEADBEEF  (4 bytes)
   [0x80200014] (padding)            (4 bytes)
   
   Step 6: Return user pointer
   ---------------------------
   user_ptr = 0x80200000 + 24 = 0x80200018
   
   User can write 10000 bytes starting at 0x80200018:
   - Bytes 0-4071    in page 0 (0x80200018 - 0x80200FFF)
   - Bytes 4072-8167 in page 1 (0x80201000 - 0x80201FFF)
   - Bytes 8168-9999 in page 2 (0x80202000 - 0x80202727)
   
   Unused: 0x80202728 - 0x80202FFF (2264 bytes wasted)

**Identity Mapping Assumption:**

Currently, ThunderOS runs with **identity mapping** (virtual address == physical address), so the PMM returns a physical address that can be used directly as a pointer:

.. code-block:: c

   // Current: Direct cast works because VA == PA
   uintptr_t page_addr = pmm_alloc_page();  // Returns 0x80200000 (physical)
   struct kmalloc_header *header = (struct kmalloc_header *)page_addr;
   // header now points to 0x80200000 (virtual == physical)

When virtual memory is enabled in the future, physical addresses from PMM must be translated to kernel virtual addresses:

.. code-block:: c

   // Future: VA != PA, need translation
   uintptr_t phys_addr = pmm_alloc_page();       // Returns 0x80200000 (physical)
   void *virt_addr = phys_to_virt(phys_addr);    // Maps to 0xFFFFFFFF80200000 (virtual)
   struct kmalloc_header *header = (struct kmalloc_header *)virt_addr;
   // header now points to virtual address that maps to physical 0x80200000

Deallocation Algorithm
~~~~~~~~~~~~~~~~~~~~~~~

The deallocation process has **four distinct steps**: validate pointer, recover header, validate header, and free pages.

.. code-block:: c

   void kfree(void *ptr) {
       // Step 1: Check for NULL (defensive programming)
       if (ptr == NULL) {
           return;  // Safe no-op, allows kfree(NULL)
       }
       
       // Step 2: Recover header by subtracting header size
       struct kmalloc_header *header = 
           (struct kmalloc_header *)((uintptr_t)ptr - HEADER_SIZE);
       
       // Step 3: Validate magic number (detect corruption)
       if (header->magic != KMALLOC_MAGIC) {
           kernel_panic("kfree: Invalid pointer or corrupted heap header");
       }
       
       // Step 4: Free page(s) back to PMM
       uintptr_t page_addr = (uintptr_t)header;
       if (header->pages == 1) {
           pmm_free_page(page_addr);              // Single page
       } else {
           pmm_free_pages(page_addr, header->pages);  // Multiple pages
       }
   }

**Step-by-step walkthrough for kfree(0x80200018):**

.. code-block:: text

   User calls: kfree(0x80200018)
   
   Step 1: Check for NULL
   ----------------------
   ptr = 0x80200018
   0x80200018 != NULL, so continue
   
   Step 2: Recover header
   ----------------------
   header = ptr - HEADER_SIZE
   header = 0x80200018 - 24
   header = 0x80200000
   
   Now header points to the kmalloc_header struct:
   
   Memory at 0x80200000:
   +0x00: size   = 256
   +0x08: pages  = 1
   +0x10: magic  = 0xDEADBEEF
   
   Step 3: Validate magic
   ----------------------
   Read header->magic at 0x80200010
   magic = 0xDEADBEEF
   0xDEADBEEF == KMALLOC_MAGIC ✓
   Header is valid!
   
   Step 4: Free pages
   ------------------
   page_addr = (uintptr_t)header = 0x80200000
   header->pages = 1
   
   Since pages == 1:
     Call pmm_free_page(0x80200000)
     PMM marks page as free in bitmap
   
   Done! Memory returned to system.

**What happens with corrupted memory:**

.. code-block:: text

   Scenario: Buffer underflow corrupted the header
   
   User has: ptr = 0x80200018
   
   User code accidentally wrote before the pointer:
   char *p = (char *)ptr;
   p[-10] = 'X';  // Writes to 0x8020000E (inside header!)
   
   This corrupts the magic field.
   
   Now user calls: kfree(0x80200018)
   
   Step 1: ptr != NULL ✓
   Step 2: header = 0x80200018 - 24 = 0x80200000
   Step 3: Read header->magic at 0x80200010
   
   Memory at 0x80200010:
   Expected: 0xDEADBEEF
   Actual:   0xDEAD58EF  (corrupted by 'X' = 0x58)
                    ^^
   
   0xDEAD58EF != 0xDEADBEEF ✗
   
   Call kernel_panic():
   "kfree: Invalid pointer or corrupted heap header"
   
   System halts immediately, preventing further corruption!

**Error Detection:**

The magic number (``0xDEADBEEF``) is a **sentinel value** that detects memory corruption. It helps catch:

1. **Double-free bugs**: After freeing, page may be reused with different data, changing magic

   .. code-block:: c
   
      void *ptr = kmalloc(256);
      kfree(ptr);  // Magic still valid, frees successfully
      kfree(ptr);  // Magic likely corrupted (page reused), PANIC!

2. **Use-after-free**: If page was reused, magic will be overwritten

   .. code-block:: c
   
      void *ptr = kmalloc(256);
      kfree(ptr);
      // ... time passes, page gets reused ...
      kfree(ptr);  // Magic invalid, PANIC!

3. **Buffer underflow**: Writing before the user pointer corrupts header

   .. code-block:: c
   
      void *ptr = kmalloc(256);
      char *p = (char *)ptr;
      p[-1] = 'X';  // Corrupts header!
      kfree(ptr);   // Magic corrupted, PANIC!

4. **Invalid pointers**: Random addresses won't have valid magic

   .. code-block:: c
   
      kfree((void *)0x12345678);  // Random address
      // Magic at 0x12345660 is garbage, PANIC!

5. **Pointer arithmetic errors**: Incorrect pointer math won't have valid header

   .. code-block:: c
   
      void *ptr = kmalloc(256);
      void *bad = (void *)((char *)ptr + 100);  // Wrong pointer!
      kfree(bad);  // Header 24 bytes before has wrong magic, PANIC!

**Why panic instead of returning an error?**

The kernel calls ``kernel_panic()`` (system halt) rather than returning an error because:

* **Memory corruption is fatal**: Cannot trust any data structures
* **Fail fast**: Detect bugs immediately during development
* **Prevent cascade failures**: Corruption spreads if allowed to continue
* **Security**: Corruption may indicate exploit attempt

**Note:** In production systems, this aggressive error checking ensures bugs are caught during development rather than causing subtle, hard-to-debug issues later.

Size Calculation
~~~~~~~~~~~~~~~~

Maximum usable allocation per page:

.. code-block:: c

   PAGE_SIZE = 4096 bytes
   HEADER_SIZE = 24 bytes
   Max user data per page = 4096 - 24 = 4072 bytes
   
   Multi-page allocations supported:
   - 8KB allocation = 2 pages
   - 16KB allocation = 4 pages
   - etc.

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
     - ✅ Allocate (8KB total)
   * - 8192 bytes
     - 8216 bytes
     - 3 pages
     - ✅ Allocate (12KB total)
   * - 100000 bytes
     - 100024 bytes
     - 25 pages
     - ✅ Allocate (100KB total)

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

**High Fragmentation**

Small allocations waste most of the page:

.. code-block:: text

   kmalloc(10) uses 4096 bytes for 10 bytes → 99.7% waste
   
**Solution:** Implement slab allocator for common small sizes.

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
* :doc:`memory` - Complete memory management and layout
* :doc:`kstring` - Helper functions using kmalloc
* :doc:`../riscv/memory_model` - RISC-V virtual memory
