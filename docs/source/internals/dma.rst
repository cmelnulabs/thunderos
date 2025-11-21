DMA Allocator
=============

The DMA (Direct Memory Access) allocator provides physically contiguous memory allocation for device drivers that need to perform DMA operations. This is a critical component for v0.3.0 "Memory Foundation" and enables device driver development in future releases.

Overview
--------

Device drivers (such as VirtIO block, network, and GPU drivers) require memory buffers that devices can directly access. These DMA buffers must meet specific requirements:

1. **Physical Contiguity**: Memory must be physically contiguous across multiple pages
2. **Physical Address Tracking**: Devices need physical addresses, not virtual addresses
3. **Cache Coherency**: Proper memory barriers must be used for device I/O
4. **Alignment**: Some devices require specific alignment (4KB, 64KB, etc.)

The standard kernel allocators (PMM and kmalloc) cannot reliably provide these guarantees:

* **PMM** allocates physical pages but doesn't track virtual addresses
* **kmalloc** allocates from kernel heap but doesn't guarantee physical contiguity for large allocations

The DMA allocator bridges this gap by providing a unified interface that guarantees both physical contiguity and address translation.

Architecture
------------

DMA Region Structure
~~~~~~~~~~~~~~~~~~~~

Each DMA allocation is tracked using a ``dma_region_t`` structure:

.. code-block:: c

   typedef struct dma_region {
       void *virt_addr;          // Virtual address (kernel can access)
       uintptr_t phys_addr;      // Physical address (for device)
       size_t size;              // Size in bytes (page-aligned)
       struct dma_region *next;  // Linked list for tracking
   } dma_region_t;

**Fields:**

* ``virt_addr``: Kernel virtual address for CPU access to the buffer
* ``phys_addr``: Physical address to program into device registers
* ``size``: Total size in bytes (rounded up to page boundaries)
* ``next``: Internal linked list pointer for allocation tracking

Memory Allocation Strategy
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The DMA allocator uses a two-tier allocation strategy:

1. **Physical Pages** from PMM (``pmm_alloc_pages()``)
   
   * Allocates contiguous physical pages
   * Guarantees physical contiguity across allocation
   * Pages are 4KB aligned by default

2. **Region Metadata** from kernel heap (``kmalloc()``)
   
   * Small structure (32 bytes) allocated from heap
   * Tracks virtual/physical mapping
   * Linked list for statistics and cleanup

Current Identity Mapping Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In ThunderOS v0.3.0, the kernel uses identity mapping where virtual addresses equal physical addresses. This simplifies the DMA allocator implementation:

.. code-block:: c

   // Allocate contiguous physical pages
   uintptr_t phys_addr = pmm_alloc_pages(num_pages);
   
   // Identity mapping: virt == phys
   void *virt_addr = (void *)phys_addr;

**Future Enhancement (v0.4.0+):**

When the kernel moves to higher-half mapping (virtual base at 0xFFFFFFFF80000000), the DMA allocator will need to:

1. Map allocated physical pages into kernel virtual space
2. Maintain separate virtual and physical address tracking
3. Handle page table updates for DMA regions

This is already designed into the API - drivers use the accessor functions and will not need changes.

API Reference
-------------

Initialization
~~~~~~~~~~~~~~

.. c:function:: void dma_init(void)

   Initialize the DMA allocator subsystem.
   
   **Preconditions:**
   
   * PMM must be initialized (``pmm_init()``)
   * Paging must be enabled (``paging_init()``)
   
   **Postconditions:**
   
   * Allocation tracking structures initialized
   * Statistics counters reset to zero
   * Ready to accept allocation requests
   
   **Called from:**
   
   * ``kernel_main()`` during boot sequence
   * After virtual memory initialization

Allocation
~~~~~~~~~~

.. c:function:: dma_region_t *dma_alloc(size_t size, uint32_t flags)

   Allocate a DMA-capable memory region.
   
   :param size: Size in bytes (will be rounded up to page size)
   :param flags: Allocation flags (bitwise OR of DMA_* constants)
   :return: Pointer to DMA region, or NULL on failure
   
   **Flags:**
   
   * ``DMA_ZERO`` (0x01): Zero the allocated memory before returning
   * ``DMA_ALIGN_4K`` (0x02): Align to 4KB boundary (default)
   * ``DMA_ALIGN_64K`` (0x04): Align to 64KB boundary (some devices)
   
   **Behavior:**
   
   1. Rounds ``size`` up to next page boundary (4KB)
   2. Calculates number of pages needed
   3. Allocates contiguous physical pages via PMM
   4. Optionally zeros memory if ``DMA_ZERO`` flag set
   5. Allocates tracking structure from kernel heap
   6. Adds region to internal linked list
   7. Updates statistics (region count, byte count)
   
   **Returns:**
   
   * Valid ``dma_region_t*`` on success
   * ``NULL`` if physical memory exhausted
   * ``NULL`` if kernel heap allocation fails
   
   **Example:**
   
   .. code-block:: c
   
      // Allocate 8KB for device descriptors, zeroed
      dma_region_t *descriptors = dma_alloc(8192, DMA_ZERO);
      if (!descriptors) {
          hal_uart_puts("Failed to allocate DMA region\n");
          return -ENOMEM;
      }
      
      // Get physical address for device
      uintptr_t desc_phys = dma_phys_addr(descriptors);
      
      // Get virtual address for CPU access
      void *desc_virt = dma_virt_addr(descriptors);

Deallocation
~~~~~~~~~~~~

.. c:function:: void dma_free(dma_region_t *region)

   Free a previously allocated DMA region.
   
   :param region: DMA region to free (from ``dma_alloc()``)
   
   **Behavior:**
   
   1. Calculates number of pages from region size
   2. Frees physical pages back to PMM
   3. Removes region from tracking linked list
   4. Updates statistics (decrements counts)
   5. Frees tracking structure back to kernel heap
   
   **Safety:**
   
   * Safe to call with ``NULL`` (no-op)
   * After calling, ``region`` pointer is invalid
   * Driver must ensure device is no longer accessing memory
   
   **Example:**
   
   .. code-block:: c
   
      // Clean up DMA buffers
      dma_free(descriptors);
      dma_free(data_buffer);

Accessor Functions
~~~~~~~~~~~~~~~~~~

.. c:function:: uintptr_t dma_phys_addr(dma_region_t *region)

   Get physical address of DMA region.
   
   :param region: DMA region
   :return: Physical address suitable for device programming
   
   **Use case:** Program this address into device registers for DMA operations.
   
   .. code-block:: c
   
      // Configure device DMA address register
      virtio_write_reg(VIRTIO_DESC_LOW, dma_phys_addr(region) & 0xFFFFFFFF);
      virtio_write_reg(VIRTIO_DESC_HIGH, dma_phys_addr(region) >> 32);

.. c:function:: void *dma_virt_addr(dma_region_t *region)

   Get virtual address of DMA region.
   
   :param region: DMA region
   :return: Virtual address for CPU access
   
   **Use case:** CPU reads/writes to DMA buffer before/after device operations.
   
   .. code-block:: c
   
      // Prepare data in DMA buffer
      uint32_t *buffer = (uint32_t *)dma_virt_addr(region);
      buffer[0] = command_id;
      buffer[1] = data_length;
      write_barrier();  // Ensure writes complete before device access

.. c:function:: size_t dma_size(dma_region_t *region)

   Get size of DMA region in bytes.
   
   :param region: DMA region
   :return: Size in bytes (page-aligned)
   
   **Note:** Size is rounded up to page boundaries, may be larger than requested.

Statistics
~~~~~~~~~~

.. c:function:: void dma_get_stats(size_t *allocated_regions, size_t *allocated_bytes)

   Get current DMA allocation statistics.
   
   :param allocated_regions: Output - number of currently allocated regions
   :param allocated_bytes: Output - total bytes allocated
   
   **Example:**
   
   .. code-block:: c
   
      size_t regions, bytes;
      dma_get_stats(&regions, &bytes);
      hal_uart_puts("DMA: ");
      kprint_dec(regions);
      hal_uart_puts(" regions, ");
      kprint_dec(bytes);
      hal_uart_puts(" bytes\n");

Usage Examples
--------------

VirtIO Descriptor Ring
~~~~~~~~~~~~~~~~~~~~~~~

Allocating a VirtIO descriptor ring with proper DMA setup:

.. code-block:: c

   #define VIRTIO_QUEUE_SIZE 128
   #define DESC_SIZE (VIRTIO_QUEUE_SIZE * 16)  // 16 bytes per descriptor
   
   // Allocate descriptor ring (must be zeroed per VirtIO spec)
   dma_region_t *desc_ring = dma_alloc(DESC_SIZE, DMA_ZERO);
   if (!desc_ring) {
       return -ENOMEM;
   }
   
   // Get virtual address for CPU initialization
   struct virtq_desc *descriptors = dma_virt_addr(desc_ring);
   
   // Initialize descriptors
   for (int i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
       descriptors[i].addr = 0;
       descriptors[i].len = 0;
       descriptors[i].flags = 0;
       descriptors[i].next = (i + 1) % VIRTIO_QUEUE_SIZE;
   }
   
   // Program device with physical address
   uintptr_t desc_phys = dma_phys_addr(desc_ring);
   virtio_write_reg(VIRTIO_QUEUE_DESC_LOW, desc_phys & 0xFFFFFFFF);
   virtio_write_reg(VIRTIO_QUEUE_DESC_HIGH, desc_phys >> 32);
   
   // Cleanup on driver unload
   dma_free(desc_ring);

Network Packet Buffers
~~~~~~~~~~~~~~~~~~~~~~~

Allocating multiple DMA buffers for network packet I/O:

.. code-block:: c

   #define NUM_RX_BUFFERS 32
   #define PACKET_SIZE 2048
   
   dma_region_t *rx_buffers[NUM_RX_BUFFERS];
   
   // Allocate receive buffers
   for (int i = 0; i < NUM_RX_BUFFERS; i++) {
       rx_buffers[i] = dma_alloc(PACKET_SIZE, 0);
       if (!rx_buffers[i]) {
           // Cleanup on failure
           for (int j = 0; j < i; j++) {
               dma_free(rx_buffers[j]);
           }
           return -ENOMEM;
       }
       
       // Program descriptor with buffer address
       uintptr_t phys = dma_phys_addr(rx_buffers[i]);
       rx_desc[i].addr = phys;
       rx_desc[i].len = PACKET_SIZE;
   }
   
   // Process received packet
   void *packet_data = dma_virt_addr(rx_buffers[buffer_idx]);
   read_barrier();  // Ensure device writes are visible
   process_packet(packet_data, packet_length);

Block Device I/O
~~~~~~~~~~~~~~~~

DMA buffer for block device read operation:

.. code-block:: c

   // Allocate 4KB sector buffer
   dma_region_t *sector_buf = dma_alloc(4096, DMA_ZERO);
   
   // Setup block device request
   struct virtio_blk_req {
       uint32_t type;      // VIRTIO_BLK_T_IN (read)
       uint32_t reserved;
       uint64_t sector;
   } *req = dma_virt_addr(request_region);
   
   req->type = VIRTIO_BLK_T_IN;
   req->sector = sector_number;
   
   // Setup descriptor chain
   desc[0].addr = dma_phys_addr(request_region);
   desc[0].len = sizeof(struct virtio_blk_req);
   desc[0].flags = VIRTQ_DESC_F_NEXT;
   desc[0].next = 1;
   
   desc[1].addr = dma_phys_addr(sector_buf);
   desc[1].len = 4096;
   desc[1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
   desc[1].next = 2;
   
   // Memory barrier before notifying device
   memory_barrier();
   virtio_notify_queue(0);
   
   // Wait for completion, then access data
   wait_for_completion();
   read_barrier();
   
   uint8_t *data = dma_virt_addr(sector_buf);
   // Process sector data...
   
   dma_free(sector_buf);

Implementation Details
----------------------

Physical Contiguity Guarantee
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The DMA allocator guarantees physical contiguity by using ``pmm_alloc_pages()`` which allocates contiguous page frames:

.. code-block:: c

   // PMM guarantees pages are physically adjacent
   uintptr_t phys_base = pmm_alloc_pages(num_pages);
   // Page 0: phys_base + 0x0000
   // Page 1: phys_base + 0x1000
   // Page 2: phys_base + 0x2000
   // ... all sequential in physical memory

This is verified by the DMA test suite (test 3):

.. code-block:: c

   // Allocate 2 pages (8KB)
   dma_region_t *region = dma_alloc(8192, 0);
   
   // Verify page 2 follows page 1
   uintptr_t page1_phys = dma_phys_addr(region);
   uintptr_t page2_phys;
   virt_to_phys(get_kernel_page_table(), 
                (uintptr_t)dma_virt_addr(region) + PAGE_SIZE,
                &page2_phys);
   
   assert(page2_phys == page1_phys + PAGE_SIZE);

Memory Zeroing
~~~~~~~~~~~~~~

When ``DMA_ZERO`` flag is set, the allocator zeroes memory before returning:

.. code-block:: c

   if (flags & DMA_ZERO) {
       uint8_t *ptr = (uint8_t *)virt_addr;
       for (size_t i = 0; i < aligned_size; i++) {
           ptr[i] = 0;
       }
   }

**Why zero?**

1. **Security**: Prevent information leakage from previous allocations
2. **Device Requirements**: Some device specs require zeroed descriptor rings
3. **Debugging**: Zero-initialized memory makes bugs more reproducible

**Performance:** Zeroing is O(n) in buffer size. For large buffers, consider:

* Not using ``DMA_ZERO`` if device overwrites entire buffer
* Zeroing only relevant portions after allocation

Allocation Tracking
~~~~~~~~~~~~~~~~~~~

All allocations are tracked in a linked list for:

1. **Statistics**: Total regions and bytes allocated
2. **Debugging**: Detect memory leaks
3. **Cleanup**: Free all regions on driver unload

.. code-block:: c

   static dma_region_t *dma_regions_head = NULL;
   
   // Add to list on allocation
   if (dma_regions_head == NULL) {
       dma_regions_head = region;
   } else {
       // Append to end
       dma_region_t *current = dma_regions_head;
       while (current->next != NULL) {
           current = current->next;
       }
       current->next = region;
   }
   
   // Remove from list on free
   if (dma_regions_head == region) {
       dma_regions_head = region->next;
   } else {
       // Find and unlink
       ...
   }

Error Handling
~~~~~~~~~~~~~~

The allocator handles two failure modes:

1. **Physical Memory Exhaustion**
   
   .. code-block:: c
   
      uintptr_t phys_addr = pmm_alloc_pages(num_pages);
      if (phys_addr == 0) {
          hal_uart_puts("dma_alloc: failed to allocate physical pages\n");
          return NULL;
      }

2. **Heap Allocation Failure**
   
   .. code-block:: c
   
      dma_region_t *region = kmalloc(sizeof(dma_region_t));
      if (region == NULL) {
          pmm_free_pages(phys_addr, num_pages);  // Free physical pages
          hal_uart_puts("dma_alloc: failed to allocate region structure\n");
          return NULL;
      }

**Important:** On heap failure, physical pages are freed to prevent leaks.
   - size:      8 bytes (size_t)
   - next:      8 bytes (pointer)

**Example:** 100 DMA regions = 3.2KB overhead

Fragmentation
~~~~~~~~~~~~~

The DMA allocator is susceptible to physical memory fragmentation:

* **Problem**: Large allocations may fail even with sufficient total free memory
* **Cause**: Physical pages become non-contiguous over time
* **Mitigation** (future):
  
  * Compaction/defragmentation
  * Reserved DMA memory pool
  * Buddy allocator integration

**Current Status (v0.3.0):**

* No defragmentation
* Allocations fail if contiguous pages unavailable
* Acceptable for early boot and limited device drivers

Testing
-------

The DMA allocator includes comprehensive tests in ``tests/test_memory_mgmt.c``:

Test Suite
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 10 30 60
   
   * - Test
     - Description
     - Verification
   * - 1
     - DMA Allocation
     - Allocate 8KB region, check non-NULL, verify addresses
   * - 2
     - Memory Zeroing
     - Check first 256 bytes are zero with DMA_ZERO flag
   * - 3
     - Physical Contiguity
     - Verify page 2 follows page 1 in physical memory
   * - 4
     - Multiple Regions
     - Allocate second region, ensure independent addresses
   * - 5
     - Address Translation
     - Verify virt-to-phys translation matches DMA region
   * - 8
     - Statistics
     - Verify region count and byte count are correct
   * - 9
     - DMA Free
     - Free region, verify statistics decrease
   * - 10
     - Complete Cleanup
     - Free all regions, verify stats return to zero

**All tests pass (10/10) in v0.3.0**

Running Tests
~~~~~~~~~~~~~

.. code-block:: bash

   cd /workspaces/thunderos
   make qemu

Tests run automatically during boot:

.. code-block:: text

   ========================================
     Memory Management Feature Tests
   ========================================
   
   Test 1: DMA Allocation
     Allocating 8KB DMA region... PASS
       Virtual:  0x0000000080258000
       Physical: 0x0000000080258000
       Size:     8192 bytes
   
   Test 2: DMA Memory is Zeroed (DMA_ZERO flag)
     Checking first 256 bytes... PASS
   
   Test 3: Physical Contiguity (2 pages)
     Verifying physical addresses are contiguous... PASS
       Page 1 physical: 0x0000000080258000
       Page 2 physical: 0x0000000080259000
   
   ...
   
   Test Summary:
     Passed: 10 / 10
     Status: ALL TESTS PASSED!

Future Enhancements
-------------------

Planned for v0.4.0 and Beyond
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. **Higher-Half Kernel Support**
   
   * Map DMA regions into kernel virtual space
   * Maintain separate virtual/physical tracking
   * Update page tables for DMA allocations

2. **64KB Alignment**
   
   * Implement DMA_ALIGN_64K flag
   * Some devices require large alignment
   * Buddy allocator integration

3. **DMA Pools**
   
   * Pre-allocated pools for common sizes
   * Reduce fragmentation
   * Faster allocation for frequent sizes

4. **IOMMU Support** (Hardware-dependent)
   
   * Virtual addressing for devices
   * Memory protection
   * Scatter-gather DMA

5. **Cache Coherency**
   
   * Explicit cache flush/invalidate
   * Support for non-coherent devices
   * Platform-specific implementations

6. **Streaming DMA**
   
   * One-shot DMA operations
   * Automatic mapping/unmapping
   * Reduced driver complexity

Related Documentation
---------------------

* :doc:`paging` - Virtual memory management
* :doc:`pmm` - Physical memory manager
* :doc:`barrier` - Memory barriers for device I/O
* :doc:`../development/device_drivers` - Writing device drivers

See Also
--------

* VirtIO Specification: https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html
* RISC-V Memory Ordering: Volume 2, Chapter 14
* Linux DMA API: Documentation/core-api/dma-api.rst
