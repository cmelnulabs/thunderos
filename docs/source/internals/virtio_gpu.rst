.. _internals-virtio-gpu:

VirtIO GPU Driver
=================

ThunderOS includes a VirtIO GPU driver for 2D framebuffer graphics output, following the VirtIO 1.0+ specification.

Overview
--------

The VirtIO GPU driver provides:

- **2D Framebuffer**: Software rendering to a pixel buffer
- **MMIO Interface**: VirtIO 1.0+ modern mode via memory-mapped I/O
- **DMA Operations**: Uses physically contiguous memory for GPU commands
- **Display Output**: Scanout support for QEMU virtual display

Architecture
------------

Device Model
~~~~~~~~~~~~

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────┐
    │                    VirtIO GPU Device                        │
    │                                                             │
    │  ┌─────────────────┐   ┌────────────────────────────────┐  │
    │  │   Control Queue │   │        Framebuffer             │  │
    │  │   (Commands)    │   │  (DMA-allocated pixel data)    │  │
    │  └────────┬────────┘   └───────────────┬────────────────┘  │
    │           │                            │                   │
    │           ▼                            ▼                   │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │              GPU Resource (ID: 1)                    │   │
    │  │  Format: B8G8R8X8_UNORM  │  Backing: fb_phys         │   │
    │  └─────────────────────────────────────────────────────┘   │
    │                            │                               │
    │                            ▼                               │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │              Scanout 0 (Display)                     │   │
    │  └─────────────────────────────────────────────────────┘   │
    └─────────────────────────────────────────────────────────────┘

Command Flow
~~~~~~~~~~~~

Rendering to the display follows this sequence:

.. code-block:: text

    1. Write pixels to framebuffer
                │
                ▼
    2. TRANSFER_TO_HOST_2D
       (Copy fb region to GPU resource)
                │
                ▼
    3. RESOURCE_FLUSH
       (Display resource on scanout)

Initialization
--------------

The driver performs VirtIO device initialization per spec:

.. code-block:: c

    int virtio_gpu_init(uintptr_t base_addr, uint32_t irq)
    {
        // 1. Reset device
        GPU_WRITE32(dev, VIRTIO_MMIO_STATUS, 0);
        
        // 2. Acknowledge device
        status = VIRTIO_STATUS_ACKNOWLEDGE;
        GPU_WRITE32(dev, VIRTIO_MMIO_STATUS, status);
        
        // 3. Set DRIVER bit
        status |= VIRTIO_STATUS_DRIVER;
        GPU_WRITE32(dev, VIRTIO_MMIO_STATUS, status);
        
        // 4. Negotiate features (reject VIRGL 3D)
        features &= ~VIRTIO_GPU_F_VIRGL;
        GPU_WRITE32(dev, VIRTIO_MMIO_DRIVER_FEATURES, features);
        
        // 5. Set FEATURES_OK
        status |= VIRTIO_STATUS_FEATURES_OK;
        GPU_WRITE32(dev, VIRTIO_MMIO_STATUS, status);
        
        // 6. Initialize control queue
        gpu_queue_init(dev, &dev->controlq, 0, queue_size);
        
        // 7. Set DRIVER_OK
        status |= VIRTIO_STATUS_DRIVER_OK;
        GPU_WRITE32(dev, VIRTIO_MMIO_STATUS, status);
        
        // 8. Query display info, create resource, attach backing
        gpu_get_display_info_internal();
        gpu_create_resource(1, format, width, height);
        gpu_attach_backing(1, fb_phys, fb_size);
        gpu_set_scanout(0, 1, width, height);
    }

Memory Layout
~~~~~~~~~~~~~

.. code-block:: text

    ┌─────────────────────────────────────┐
    │       virtio_gpu_device_t           │  (kmalloc'd)
    │  - base_addr, irq, version          │
    │  - controlq, cursorq                │
    │  - fb_pixels, fb_phys, fb_size      │
    │  - resource_id, displays[]          │
    └─────────────────────────────────────┘

    ┌─────────────────────────────────────┐
    │         Control Queue               │  (DMA regions)
    │  - Descriptor ring (64 entries)     │
    │  - Available ring                   │
    │  - Used ring                        │
    └─────────────────────────────────────┘

    ┌─────────────────────────────────────┐
    │         Framebuffer                 │  (DMA region)
    │  - width × height × 4 bytes         │
    │  - Format: B8G8R8X8_UNORM           │
    │  - Default: 800×600 = 1.9MB         │
    └─────────────────────────────────────┘

GPU Commands
------------

The driver implements these VirtIO GPU commands:

Command Types
~~~~~~~~~~~~~

.. list-table:: Supported GPU Commands
   :header-rows: 1
   :widths: 35 15 50

   * - Command
     - Code
     - Description
   * - ``GET_DISPLAY_INFO``
     - 0x0100
     - Query available scanouts and resolutions
   * - ``RESOURCE_CREATE_2D``
     - 0x0101
     - Create 2D resource with format and dimensions
   * - ``RESOURCE_ATTACH_BACKING``
     - 0x0106
     - Attach framebuffer memory to resource
   * - ``SET_SCANOUT``
     - 0x0103
     - Connect resource to display output
   * - ``TRANSFER_TO_HOST_2D``
     - 0x0105
     - Copy framebuffer region to GPU resource
   * - ``RESOURCE_FLUSH``
     - 0x0104
     - Trigger display update for region

Command/Response Protocol
~~~~~~~~~~~~~~~~~~~~~~~~~

All GPU commands use descriptor chains:

.. code-block:: text

    ┌──────────────────┐     ┌──────────────────┐
    │  Descriptor 0    │────▶│  Descriptor 1    │
    │  (Command)       │     │  (Response)      │
    │  flags: NEXT     │     │  flags: WRITE    │
    └──────────────────┘     └──────────────────┘

.. code-block:: c

    static int gpu_send_command(void *cmd, size_t cmd_size,
                                void *resp, size_t resp_size)
    {
        // Get physical addresses for DMA
        uintptr_t cmd_phys = translate_virt_to_phys((uintptr_t)cmd);
        uintptr_t resp_phys = translate_virt_to_phys((uintptr_t)resp);
        
        // Setup descriptor chain
        desc[0].addr = cmd_phys;
        desc[0].len = cmd_size;
        desc[0].flags = VIRTQ_DESC_F_NEXT;
        desc[0].next = 1;
        
        desc[1].addr = resp_phys;
        desc[1].len = resp_size;
        desc[1].flags = VIRTQ_DESC_F_WRITE;
        
        // Notify device and wait for completion
        GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_CONTROL);
        
        // Poll used ring for response
        while (vq->last_seen_used == vq->used->idx) {
            // ... timeout handling
        }
    }

Public API
----------

Initialization & Status
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    // Initialize GPU at MMIO address
    int virtio_gpu_init(uintptr_t base_addr, uint32_t irq);
    
    // Check if GPU is available
    int virtio_gpu_available(void);
    
    // Shutdown GPU
    void virtio_gpu_shutdown(void);

Framebuffer Access
~~~~~~~~~~~~~~~~~~

.. code-block:: c

    // Direct framebuffer pointer (for bulk operations)
    uint32_t *virtio_gpu_get_framebuffer(void);
    
    // Get framebuffer dimensions
    void virtio_gpu_get_dimensions(uint32_t *width, uint32_t *height);
    
    // Get display info
    int virtio_gpu_get_display_info(uint32_t *width, uint32_t *height);

Pixel Operations
~~~~~~~~~~~~~~~~

.. code-block:: c

    // Set pixel (ARGB format)
    void virtio_gpu_set_pixel(uint32_t x, uint32_t y, uint32_t color);
    
    // Get pixel (returns ARGB)
    uint32_t virtio_gpu_get_pixel(uint32_t x, uint32_t y);
    
    // Clear entire framebuffer
    void virtio_gpu_clear(uint32_t color);

Display Update
~~~~~~~~~~~~~~

.. code-block:: c

    // Flush entire framebuffer to display
    int virtio_gpu_flush(void);
    
    // Flush specific region (more efficient)
    int virtio_gpu_flush_region(uint32_t x, uint32_t y,
                                uint32_t width, uint32_t height);

Usage Example
-------------

.. code-block:: c

    // Initialize GPU
    if (virtio_gpu_init(GPU_MMIO_BASE, GPU_IRQ) < 0) {
        kprintf("GPU init failed\n");
        return -1;
    }
    
    // Get dimensions
    uint32_t w, h;
    virtio_gpu_get_dimensions(&w, &h);
    
    // Clear to blue
    virtio_gpu_clear(0xFF0000FF);
    
    // Draw red rectangle
    for (uint32_t y = 100; y < 200; y++) {
        for (uint32_t x = 100; x < 300; x++) {
            virtio_gpu_set_pixel(x, y, 0xFFFF0000);
        }
    }
    
    // Update display
    virtio_gpu_flush();

Pixel Format
------------

Color Conversion
~~~~~~~~~~~~~~~~

The driver uses ARGB internally but the GPU expects BGRX:

.. code-block:: c

    // User provides: 0xAARRGGBB (ARGB)
    // GPU expects:   0xBBGGRR00 (BGRX)
    
    void virtio_gpu_set_pixel(uint32_t x, uint32_t y, uint32_t color)
    {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        
        // Convert to B8G8R8X8_UNORM (actually RGBX in memory)
        uint32_t bgrx = (r << 16) | (g << 8) | b;
        fb_pixels[y * width + x] = bgrx;
    }

Supported Formats
~~~~~~~~~~~~~~~~~

.. list-table:: Pixel Formats
   :header-rows: 1
   :widths: 40 20 40

   * - Format
     - Code
     - Description
   * - ``B8G8R8X8_UNORM``
     - 2
     - 32-bit BGRX (used by driver)
   * - ``B8G8R8A8_UNORM``
     - 1
     - 32-bit BGRA with alpha
   * - ``R8G8B8A8_UNORM``
     - 67
     - 32-bit RGBA

Error Handling
--------------

The driver uses ThunderOS errno system:

.. code-block:: c

    // Possible errors:
    THUNDEROS_ENOMEM           // DMA allocation failed
    THUNDEROS_ENODEV           // GPU not initialized
    THUNDEROS_EVIRTIO_TIMEOUT  // Command timeout
    THUNDEROS_EVIRTIO_BADDEV   // Invalid VirtIO device
    THUNDEROS_EIO              // GPU returned error response

Error Response Types
~~~~~~~~~~~~~~~~~~~~

.. list-table:: GPU Error Responses
   :header-rows: 1
   :widths: 40 20 40

   * - Response
     - Code
     - Meaning
   * - ``RESP_ERR_UNSPEC``
     - 0x1200
     - Unspecified error
   * - ``RESP_ERR_OUT_OF_MEMORY``
     - 0x1201
     - GPU memory exhausted
   * - ``RESP_ERR_INVALID_SCANOUT_ID``
     - 0x1202
     - Bad scanout index
   * - ``RESP_ERR_INVALID_RESOURCE_ID``
     - 0x1203
     - Bad resource ID

QEMU Configuration
------------------

To use VirtIO GPU with QEMU:

.. code-block:: bash

    qemu-system-riscv64 \
        -machine virt \
        -m 128M \
        -kernel build/thunderos.elf \
        -device virtio-gpu-device \
        -display gtk

Or for headless with VNC:

.. code-block:: bash

    qemu-system-riscv64 \
        -machine virt \
        -m 128M \
        -kernel build/thunderos.elf \
        -device virtio-gpu-device \
        -vnc :0

Device Discovery
~~~~~~~~~~~~~~~~

The VirtIO GPU appears at MMIO address ``0x10008000`` on QEMU virt machine (third VirtIO slot after block device).

Implementation Details
----------------------

Source Files
~~~~~~~~~~~~

.. code-block:: text

    include/drivers/virtio_gpu.h    # Data structures and API
    kernel/drivers/virtio_gpu.c     # Driver implementation

Key Structures
~~~~~~~~~~~~~~

.. code-block:: c

    typedef struct {
        uintptr_t base_addr;        // MMIO base
        uint32_t irq;               // IRQ number
        uint64_t features;          // Negotiated features
        
        virtio_gpu_queue_t controlq;  // Command queue
        virtio_gpu_queue_t cursorq;   // Cursor queue (unused)
        
        uint32_t fb_width;          // Framebuffer width
        uint32_t fb_height;         // Framebuffer height
        uint32_t *fb_pixels;        // Pixel buffer (virtual)
        uintptr_t fb_phys;          // Pixel buffer (physical)
        size_t fb_size;             // Buffer size
        uint32_t resource_id;       // GPU resource ID
        
        uint32_t flush_count;       // Statistics
        uint32_t error_count;
    } virtio_gpu_device_t;

Limitations
-----------

Current implementation limitations:

- **2D Only**: No 3D/VIRGL support (deliberately disabled)
- **Single Resource**: Only one framebuffer resource (ID 1)
- **Single Scanout**: Only scanout 0 used
- **No Cursor**: Cursor queue not implemented
- **No Interrupt-Driven I/O**: Uses polling for command completion
- **Fixed Format**: Always uses B8G8R8X8_UNORM

Future Enhancements
-------------------

Planned improvements:

- Hardware cursor support
- Multiple display (scanout) support
- Interrupt-driven command completion
- Console rendering with bitmap fonts
- Integration with virtual terminal system
- Window manager primitives

See Also
--------

- :doc:`virtio_block` - VirtIO block device driver
- :doc:`dma` - DMA allocator used for GPU buffers
- :doc:`virtual_terminals` - Future GPU-based terminal rendering
