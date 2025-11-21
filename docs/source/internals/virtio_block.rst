VirtIO Block Driver
===================

Overview
--------

The VirtIO block driver (``kernel/drivers/virtio_blk.c``) provides a high-performance storage interface for ThunderOS. It implements the VirtIO specification for block devices, supporting both modern (MMIO-based) and legacy (PFN-based) addressing modes.

VirtIO is a standardized I/O virtualization framework that allows efficient device access in virtualized environments like QEMU. The block driver provides the foundation for the filesystem layer by offering sector-based read/write operations.

Architecture
------------

Memory-Mapped I/O Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The VirtIO device is accessed through memory-mapped I/O (MMIO) registers at physical address ``0x10008000``. The driver communicates with the device by reading and writing to these registers:

**Key MMIO Registers:**

.. code-block:: c

    #define VIRTIO_MMIO_MAGIC_VALUE         0x000  // Should be 0x74726976
    #define VIRTIO_MMIO_VERSION             0x004  // Device version (1 or 2)
    #define VIRTIO_MMIO_DEVICE_ID           0x008  // Device type (2 = block)
    #define VIRTIO_MMIO_VENDOR_ID           0x00c  // Vendor ID
    #define VIRTIO_MMIO_DEVICE_FEATURES     0x010  // Device capability flags
    #define VIRTIO_MMIO_QUEUE_SEL           0x030  // Select queue number
    #define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034  // Maximum queue size
    #define VIRTIO_MMIO_QUEUE_NUM           0x038  // Current queue size
    #define VIRTIO_MMIO_QUEUE_READY         0x044  // Queue activation status
    #define VIRTIO_MMIO_QUEUE_NOTIFY        0x050  // Trigger device processing
    #define VIRTIO_MMIO_INTERRUPT_STATUS    0x060  // Interrupt status flags
    #define VIRTIO_MMIO_INTERRUPT_ACK       0x064  // Acknowledge interrupts
    #define VIRTIO_MMIO_STATUS              0x070  // Device status
    #define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080  // Descriptor table address (low)
    #define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084  // Descriptor table address (high)
    #define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090  // Available ring address (low)
    #define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094  // Available ring address (high)
    #define VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0  // Used ring address (low)
    #define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4  // Used ring address (high)

Descriptor Ring Architecture
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VirtIO uses a ring buffer mechanism for transferring data between the driver and device. The ring consists of three parts:

1. **Descriptor Table**: Describes memory buffers (address, length, flags)
2. **Available Ring**: Descriptors the driver has made available to the device
3. **Used Ring**: Descriptors the device has finished processing

.. code-block:: text

    ┌─────────────────────────────────────────────────┐
    │         VirtIO Descriptor Ring Layout           │
    ├─────────────────────────────────────────────────┤
    │                                                 │
    │  ┌────────────────────────────────────────┐    │
    │  │  Descriptor Table (256 descriptors)    │    │
    │  │  Each: addr (64-bit)                   │    │
    │  │        len  (32-bit)                   │    │
    │  │        flags (16-bit)                  │    │
    │  │        next (16-bit)                   │    │
    │  └────────────────────────────────────────┘    │
    │                    ↓                            │
    │  ┌────────────────────────────────────────┐    │
    │  │  Available Ring                        │    │
    │  │  flags, idx, ring[256], used_event     │    │
    │  └────────────────────────────────────────┘    │
    │                    ↓                            │
    │  ┌────────────────────────────────────────┐    │
    │  │  Used Ring                             │    │
    │  │  flags, idx, ring[256], avail_event    │    │
    │  └────────────────────────────────────────┘    │
    │                                                 │
    └─────────────────────────────────────────────────┘

**Descriptor Chaining:**

Complex operations (like read/write) require multiple descriptors chained together:

- Descriptor 0: VirtIO block request header (device-readable)
- Descriptor 1: Data buffer (device-writable for reads, readable for writes)
- Descriptor 2: Status byte (device-writable, reports success/failure)

Data Structures
---------------

VirtIO Descriptor
~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct virtq_desc {
        uint64_t addr;   // Physical address of buffer
        uint32_t len;    // Length of buffer in bytes
        uint16_t flags;  // Flags (NEXT, WRITE, INDIRECT)
        uint16_t next;   // Index of next descriptor in chain
    };

**Flags:**

- ``VIRTQ_DESC_F_NEXT (1)``: This descriptor continues in next descriptor
- ``VIRTQ_DESC_F_WRITE (2)``: Device writes to this buffer (vs reads from it)
- ``VIRTQ_DESC_F_INDIRECT (4)``: Buffer contains a list of descriptors

Available Ring
~~~~~~~~~~~~~~

.. code-block:: c

    struct virtq_avail {
        uint16_t flags;         // Suppress interrupts if set
        uint16_t idx;           // Next slot to be filled
        uint16_t ring[QUEUE_SIZE];  // Descriptor indices
        uint16_t used_event;    // Used for event suppression
    };

The driver adds descriptor indices to ``ring[idx % QUEUE_SIZE]`` and increments ``idx``.

Used Ring
~~~~~~~~~

.. code-block:: c

    struct virtq_used_elem {
        uint32_t id;   // Descriptor chain head index
        uint32_t len;  // Bytes written to buffer
    };

    struct virtq_used {
        uint16_t flags;
        uint16_t idx;           // Next slot device will use
        struct virtq_used_elem ring[QUEUE_SIZE];
        uint16_t avail_event;
    };

The device adds completed descriptors to ``ring[idx % QUEUE_SIZE]`` and increments ``idx``.

VirtIO Block Request
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct virtio_blk_req {
        uint32_t type;          // VIRTIO_BLK_T_IN (read) or T_OUT (write)
        uint32_t reserved;      // Must be zero
        uint64_t sector;        // Sector number (512-byte units)
    };

**Request Types:**

- ``VIRTIO_BLK_T_IN (0)``: Read from device
- ``VIRTIO_BLK_T_OUT (1)``: Write to device
- ``VIRTIO_BLK_T_FLUSH (4)``: Flush cache

VirtIO Block Status
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #define VIRTIO_BLK_S_OK        0  // Success
    #define VIRTIO_BLK_S_IOERR     1  // I/O error
    #define VIRTIO_BLK_S_UNSUPP    2  // Unsupported operation

Error Handling
~~~~~~~~~~~~~~

VirtIO operations use ThunderOS's errno system. The driver sets specific error codes for different failure conditions:

**VirtIO Error Codes:**

.. code-block:: c

    #define THUNDEROS_EVIRTIO_TIMEOUT  71  /* I/O operation timeout */
    #define THUNDEROS_EVIRTIO_NODEV    72  /* Device not found/initialized */
    #define THUNDEROS_EVIRTIO_INIT     73  /* Initialization failed */

**Common Error Scenarios:**

1. **Timeout** (``THUNDEROS_EVIRTIO_TIMEOUT``):
   - Device doesn't respond within timeout period (default: 100,000 iterations)
   - Most common cause: missing ``-global virtio-mmio.force-legacy=false`` flag
   - Check QEMU configuration if this error occurs

2. **Device not found** (``THUNDEROS_EVIRTIO_NODEV``):
   - No VirtIO device at expected MMIO address (0x10001000)
   - Wrong device type (not a block device)
   - QEMU not started with ``-device virtio-blk-device``

3. **Initialization failed** (``THUNDEROS_EVIRTIO_INIT``):
   - Feature negotiation failed
   - Queue setup failed
   - Device status indicates error

**Example:**

.. code-block:: c

    char buffer[512];
    if (virtio_blk_read(0, buffer, 1) < 0) {
        int err = get_errno();
        if (err == THUNDEROS_EVIRTIO_TIMEOUT) {
            kprintf("VirtIO timeout - check QEMU flags!\n");
        } else if (err == THUNDEROS_EVIRTIO_NODEV) {
            kprintf("VirtIO device not found\n");
        }
        return -1;  // errno already set
    }

See ``docs/source/internals/errno.rst`` for complete errno documentation.

Initialization Sequence
-----------------------

The driver initialization follows the VirtIO specification's device initialization protocol:

.. code-block:: c

    int virtio_blk_init(void) {
        // 1. Reset device
        write32(VIRTIO_MMIO_STATUS, 0);
        
        // 2. Set ACKNOWLEDGE bit (OS recognizes device)
        write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
        
        // 3. Set DRIVER bit (OS has driver for device)
        uint32_t status = read32(VIRTIO_MMIO_STATUS);
        write32(VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_DRIVER);
        
        // 4. Read device features
        uint32_t features = read32(VIRTIO_MMIO_DEVICE_FEATURES);
        
        // 5. Write understood features (negotiate)
        write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);
        
        // 6. Set FEATURES_OK bit
        status = read32(VIRTIO_MMIO_STATUS);
        write32(VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FEATURES_OK);
        
        // 7. Re-read status to verify FEATURES_OK still set
        status = read32(VIRTIO_MMIO_STATUS);
        if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
            return -1;  // Feature negotiation failed
        }
        
        // 8. Allocate and initialize virtqueue
        virtio_blk_setup_queue();
        
        // 9. Set DRIVER_OK bit (ready for operation)
        status = read32(VIRTIO_MMIO_STATUS);
        write32(VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_DRIVER_OK);
        
        return 0;
    }

**Status Bits:**

- ``VIRTIO_STATUS_ACKNOWLEDGE (1)``: Guest OS has recognized the device
- ``VIRTIO_STATUS_DRIVER (2)``: Guest OS has a driver for this device
- ``VIRTIO_STATUS_DRIVER_OK (4)``: Driver is ready and device can be used
- ``VIRTIO_STATUS_FEATURES_OK (8)``: Feature negotiation successful
- ``VIRTIO_STATUS_FAILED (128)``: Device encountered an unrecoverable error

Queue Setup
~~~~~~~~~~~

The virtqueue is allocated from DMA-capable memory:

.. code-block:: c

    static int virtio_blk_setup_queue(void) {
        // Select queue 0
        write32(VIRTIO_MMIO_QUEUE_SEL, 0);
        
        // Check maximum queue size supported
        uint32_t max_queue_size = read32(VIRTIO_MMIO_QUEUE_NUM_MAX);
        if (max_queue_size < QUEUE_SIZE) {
            return -1;
        }
        
        // Set queue size
        write32(VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
        
        // Allocate physically contiguous memory
        size_t total_size = vring_size(QUEUE_SIZE, PAGE_SIZE);
        void *queue_mem = dma_alloc(total_size, DMA_ZERO);
        uint64_t queue_phys = translate_virt_to_phys((uint64_t)queue_mem);
        
        // Initialize vring structure
        vring_init(&vring, QUEUE_SIZE, queue_mem, PAGE_SIZE);
        
        // Write queue addresses to device (modern MMIO)
        write32(VIRTIO_MMIO_QUEUE_DESC_LOW, queue_phys & 0xFFFFFFFF);
        write32(VIRTIO_MMIO_QUEUE_DESC_HIGH, queue_phys >> 32);
        
        uint64_t avail_phys = queue_phys + ((uint64_t)vring.avail - (uint64_t)queue_mem);
        write32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_phys & 0xFFFFFFFF);
        write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_phys >> 32);
        
        uint64_t used_phys = queue_phys + ((uint64_t)vring.used - (uint64_t)queue_mem);
        write32(VIRTIO_MMIO_QUEUE_USED_LOW, used_phys & 0xFFFFFFFF);
        write32(VIRTIO_MMIO_QUEUE_USED_HIGH, used_phys >> 32);
        
        // Activate queue
        write32(VIRTIO_MMIO_QUEUE_READY, 1);
        
        return 0;
    }

I/O Operations
--------------

Read Operation
~~~~~~~~~~~~~~

Reading a sector from the block device:

.. code-block:: c

    int virtio_blk_read(uint64_t sector, void *buffer, size_t count) {
        // 1. Allocate descriptor chain
        uint16_t desc_req = alloc_desc();    // Request header
        uint16_t desc_data = alloc_desc();   // Data buffer
        uint16_t desc_status = alloc_desc(); // Status byte
        
        // 2. Setup request header (device reads this)
        struct virtio_blk_req *req = &request_buffers[desc_req];
        req->type = VIRTIO_BLK_T_IN;  // Read operation
        req->sector = sector;
        
        vring.desc[desc_req].addr = translate_virt_to_phys((uint64_t)req);
        vring.desc[desc_req].len = sizeof(struct virtio_blk_req);
        vring.desc[desc_req].flags = VIRTQ_DESC_F_NEXT;
        vring.desc[desc_req].next = desc_data;
        
        // 3. Setup data buffer (device writes here)
        vring.desc[desc_data].addr = translate_virt_to_phys((uint64_t)buffer);
        vring.desc[desc_data].len = count * 512;
        vring.desc[desc_data].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
        vring.desc[desc_data].next = desc_status;
        
        // 4. Setup status byte (device writes result)
        vring.desc[desc_status].addr = translate_virt_to_phys((uint64_t)&status_buffers[desc_req]);
        vring.desc[desc_status].len = 1;
        vring.desc[desc_status].flags = VIRTQ_DESC_F_WRITE;
        
        // 5. Add to available ring
        uint16_t avail_idx = vring.avail->idx;
        vring.avail->ring[avail_idx % QUEUE_SIZE] = desc_req;
        memory_barrier();  // Ensure descriptor writes complete
        vring.avail->idx = avail_idx + 1;
        memory_barrier();  // Ensure index write visible
        
        // 6. Notify device
        write32(VIRTIO_MMIO_QUEUE_NOTIFY, 0);
        
        // 7. Wait for completion (polling)
        while (last_used_idx == vring.used->idx) {
            // Spin wait (could yield to scheduler here)
        }
        
        // 8. Process used ring
        struct virtq_used_elem *used_elem = &vring.used->ring[last_used_idx % QUEUE_SIZE];
        uint8_t status = status_buffers[used_elem->id];
        last_used_idx++;
        
        // 9. Free descriptors
        free_desc(desc_req);
        free_desc(desc_data);
        free_desc(desc_status);
        
        return (status == VIRTIO_BLK_S_OK) ? 0 : -1;
    }

Write Operation
~~~~~~~~~~~~~~~

Writing to a sector is similar, but the data buffer is device-readable:

.. code-block:: c

    int virtio_blk_write(uint64_t sector, const void *buffer, size_t count) {
        // Setup is identical except:
        req->type = VIRTIO_BLK_T_OUT;  // Write operation
        
        // Data descriptor is device-readable (no WRITE flag)
        vring.desc[desc_data].flags = VIRTQ_DESC_F_NEXT;  // No WRITE flag
        
        // Rest of process is identical...
    }

Memory Barriers
---------------

The driver uses memory barriers extensively to ensure correct ordering of operations:

.. code-block:: c

    // After writing descriptors, before updating available index
    memory_barrier();  // Ensures descriptor writes complete first
    
    vring.avail->idx = new_idx;
    
    // After updating available index, before notifying device
    memory_barrier();  // Ensures index write is visible
    
    write32(VIRTIO_MMIO_QUEUE_NOTIFY, 0);

**Why Barriers Matter:**

- CPU may reorder memory writes for performance
- Device DMA may see incomplete descriptor chains
- Without barriers, device could process partial requests
- RISC-V ``fence`` instruction enforces ordering

DMA Considerations
------------------

Physical Address Translation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All addresses given to the VirtIO device must be physical addresses:

.. code-block:: c

    // Convert virtual buffer address to physical
    uint64_t phys_addr = translate_virt_to_phys((uint64_t)buffer);
    
    // Use physical address in descriptor
    vring.desc[i].addr = phys_addr;

The ``translate_virt_to_phys()`` function walks the page tables to find the physical frame.

Contiguous Memory
~~~~~~~~~~~~~~~~~

VirtIO ring buffers must be physically contiguous:

.. code-block:: c

    // Allocate from DMA allocator (guarantees contiguity)
    void *ring = dma_alloc(vring_size(QUEUE_SIZE, PAGE_SIZE), DMA_ZERO);

Regular ``kmalloc()`` cannot guarantee physical contiguity for multi-page allocations.

Cache Coherency
~~~~~~~~~~~~~~~

RISC-V does not require explicit cache flushes for DMA on most implementations (cache-coherent DMA). However, memory barriers ensure proper ordering.

Debugging
---------

Common Issues
~~~~~~~~~~~~~

**Device Not Found:**

.. code-block:: c

    if (read32(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976) {
        kprintf("No VirtIO device at 0x%p\n", VIRTIO_MMIO_BASE);
    }

Check QEMU command line includes ``-device virtio-blk-device``.

**Feature Negotiation Failure:**

.. code-block:: c

    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("Feature negotiation failed\n");
        uint32_t features = read32(VIRTIO_MMIO_DEVICE_FEATURES);
        kprintf("Device features: 0x%x\n", features);
    }

Device may require features the driver doesn't support.

**I/O Timeout:**

.. code-block:: c

    int timeout = 10000;
    while (last_used_idx == vring.used->idx && timeout-- > 0) {
        // Wait with timeout
    }
    
    if (timeout == 0) {
        kprintf("VirtIO I/O timeout!\n");
        kprintf("last_used_idx=%d, vring.used->idx=%d\n",
                last_used_idx, vring.used->idx);
    }

Check descriptor flags (WRITE flag for device-writable buffers).

**Descriptor Exhaustion:**

.. code-block:: c

    if (num_free_desc == 0) {
        kprintf("No free descriptors!\n");
        // Wait for pending requests to complete
    }

Increase ``QUEUE_SIZE`` or implement request queuing.

Testing
-------

QEMU Setup
~~~~~~~~~~

Create a test disk image:

.. code-block:: bash

    # Create 10MB disk image
    dd if=/dev/zero of=disk.img bs=1M count=10
    
    # Format with ext2
    mkfs.ext2 disk.img
    
    # Run QEMU with VirtIO disk
    qemu-system-riscv64 \
        -machine virt \
        -m 128M \
        -kernel thunderos.elf \
        -drive file=disk.img,if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -global virtio-mmio.force-legacy=false \
        -nographic

.. warning::
   **CRITICAL: QEMU 10.1.2+ Configuration**
   
   Modern VirtIO requires the ``-global virtio-mmio.force-legacy=false`` flag.
   
   **Without this flag:**
   - All VirtIO I/O operations will timeout
   - Mount will fail with "VirtIO timeout" errors
   - Errno will be set to ``THUNDEROS_EVIRTIO_TIMEOUT`` (71)
   
   **Why this flag is needed:**
   QEMU 10+ defaults to legacy mode for MMIO devices. ThunderOS implements
   modern VirtIO (v2) with 64-bit queue addressing. The flag forces QEMU to
   use modern mode, matching our driver implementation.
   
   **Symptoms of missing flag:**
   
   .. code-block:: text
   
       [FAIL] VirtIO block device timeout
       [FAIL] Failed to mount ext2 filesystem
       virtio_blk_do_request: Timeout waiting for response
   
   Always include this flag when running ThunderOS with VirtIO devices!

Verification
~~~~~~~~~~~~

At boot, the driver should print:

.. code-block:: text

    [OK] VirtIO block device initialized
    Device capacity: 20480 sectors (10 MB)

Test read operations:

.. code-block:: c

    char buffer[512];
    if (virtio_blk_read(0, buffer, 1) == 0) {
        kprintf("Successfully read sector 0\n");
    }

References
----------

- `VirtIO Specification v1.1 <https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html>`_
- `RISC-V Memory Model <https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf>`_
- ThunderOS DMA allocator: ``docs/source/internals/dma.rst``
- ThunderOS memory barriers: ``docs/source/internals/barrier.rst``

Implementation Files
--------------------

- ``kernel/drivers/virtio_blk.c`` - Driver implementation
- ``include/hal/virtio_blk.h`` - Public API and constants
- ``kernel/mm/dma.c`` - DMA allocator (used for ring buffers)
- ``kernel/mm/paging.c`` - Address translation functions
