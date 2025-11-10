Memory Barriers
===============

Memory barriers (also called memory fences) are synchronization primitives that enforce ordering constraints on memory operations. They are essential for correct device driver implementation and multi-core synchronization in ThunderOS.

Overview
--------

Why Memory Barriers?
~~~~~~~~~~~~~~~~~~~~

Modern CPUs and compilers reorder memory operations for performance. While this is safe for single-threaded code, it can cause bugs in:

1. **Device I/O**: Hardware devices see memory operations in unexpected order
2. **Multi-core Code**: Other CPUs may observe inconsistent memory state
3. **Interrupt Handlers**: Shared data between interrupt and main contexts

Consider this example without barriers:

.. code-block:: c

   // Setup DMA descriptor
   descriptor->addr = buffer_phys;    // Write 1
   descriptor->length = 4096;         // Write 2
   descriptor->flags = DESC_READY;    // Write 3
   
   // Notify device
   virtio_notify_queue(0);            // Write 4

**Problem:** CPU or compiler might reorder writes, so device sees:

* Write 4 (notify) happens before Write 3 (flags)
* Device reads descriptor before it's fully initialized
* **Result:** Corrupt DMA operation

**Solution:** Memory barrier between setup and notification:

.. code-block:: c

   // Setup DMA descriptor
   descriptor->addr = buffer_phys;
   descriptor->length = 4096;
   descriptor->flags = DESC_READY;
   
   memory_barrier();  // Ensure all writes complete
   
   virtio_notify_queue(0);  // Now safe

RISC-V Memory Model
~~~~~~~~~~~~~~~~~~~

RISC-V uses a weak memory model called **RVWMO** (RISC-V Weak Memory Ordering):

* Loads and stores can be reordered by hardware
* Only synchronization instructions enforce ordering
* Different from x86's strong memory model

**Key Points:**

* **Within a hart (CPU core)**: Memory appears ordered as written
* **Between harts**: Ordering is NOT guaranteed without fences
* **With devices**: Ordering is NOT guaranteed without fences

RISC-V provides the ``fence`` instruction to enforce ordering:

.. code-block:: asm

   fence predecessor, successor

Where predecessor/successor can be:

* **r**: Reads (loads)
* **w**: Writes (stores)
* **rw**: Reads and writes
* **i**: Input (device reads)
* **o**: Output (device writes)

Example:

.. code-block:: asm

   fence w, w   # All writes before complete before writes after
   fence rw, rw # Full barrier: all memory ops ordered
   fence.i      # Instruction fence: for self-modifying code

API Reference
-------------

Full Memory Barriers
~~~~~~~~~~~~~~~~~~~~

.. c:function:: void memory_barrier(void)

   Full memory barrier - orders all memory operations.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void memory_barrier(void) {
          asm volatile("fence rw, rw" ::: "memory");
      }
   
   **Semantics:**
   
   * All loads/stores before the barrier complete before any loads/stores after
   * Equivalent to ``fence rw, rw``
   * Most conservative barrier - use when unsure
   
   **Use cases:**
   
   * General device I/O synchronization
   * Critical sections in multi-core code
   * When exact ordering requirements are complex
   
   **Example:**
   
   .. code-block:: c
   
      // Update shared structure atomically
      spin_lock(&lock);
      
      shared_data->field1 = value1;
      shared_data->field2 = value2;
      memory_barrier();  // Ensure writes visible to other cores
      shared_data->ready = 1;
      
      spin_unlock(&lock);

.. c:function:: void io_barrier(void)

   I/O memory barrier - for memory-mapped I/O operations.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void io_barrier(void) {
          asm volatile("fence rw, rw" ::: "memory");
      }
   
   **Semantics:**
   
   * Currently equivalent to ``memory_barrier()`` on RISC-V
   * Semantic distinction for clarity in device drivers
   * Future platforms might have different implementation
   
   **Use cases:**
   
   * After writing device registers
   * Before reading device status
   * Between related MMIO operations
   
   **Example:**
   
   .. code-block:: c
   
      // Write command to device register
      *DEVICE_COMMAND_REG = CMD_START;
      io_barrier();
      
      // Read status register
      status = *DEVICE_STATUS_REG;

Directional Barriers
~~~~~~~~~~~~~~~~~~~~

.. c:function:: void write_barrier(void)

   Write barrier - orders write operations only.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void write_barrier(void) {
          asm volatile("fence w, w" ::: "memory");
      }
   
   **Semantics:**
   
   * All stores before complete before stores after
   * More efficient than full barrier
   * Does NOT order reads
   
   **Use cases:**
   
   * Writing multiple device registers in sequence
   * Setting up DMA descriptors before notification
   * Ensuring write order to device
   
   **Example:**
   
   .. code-block:: c
   
      // Setup descriptor ring
      for (int i = 0; i < RING_SIZE; i++) {
          ring[i].addr = buffers[i];
          ring[i].len = BUFFER_SIZE;
          ring[i].flags = 0;
      }
      
      write_barrier();  // Ensure all descriptors written
      
      // Enable ring
      *DEVICE_RING_ENABLE = 1;

.. c:function:: void read_barrier(void)

   Read barrier - orders read operations only.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void read_barrier(void) {
          asm volatile("fence r, r" ::: "memory");
      }
   
   **Semantics:**
   
   * All loads before complete before loads after
   * More efficient than full barrier
   * Does NOT order writes
   
   **Use cases:**
   
   * Reading multiple device registers in sequence
   * Ensuring DMA completion before reading buffers
   * Reading shared data structures
   
   **Example:**
   
   .. code-block:: c
   
      // Wait for device completion
      while (!(*DEVICE_STATUS_REG & STATUS_DONE))
          ;
      
      read_barrier();  // Ensure status read before buffer reads
      
      // Read DMA buffer
      data = *dma_buffer;

Data Barriers
~~~~~~~~~~~~~

.. c:function:: void data_memory_barrier(void)

   Data memory barrier for DMA operations.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void data_memory_barrier(void) {
          asm volatile("fence rw, rw" ::: "memory");
      }
   
   **Semantics:**
   
   * Ensures all memory accesses observed by other agents (devices, CPUs)
   * Critical for DMA where device accesses memory directly
   * Equivalent to full barrier on RISC-V
   
   **Use cases:**
   
   * Before starting DMA (ensure buffer contents visible to device)
   * After DMA completion (ensure device writes visible to CPU)
   * Multi-core shared memory
   
   **Example:**
   
   .. code-block:: c
   
      // Fill DMA buffer
      memcpy(dma_buffer, data, size);
      
      data_memory_barrier();  // Ensure buffer writes visible to device
      
      // Start DMA
      start_dma_transfer(dma_phys_addr, size);

.. c:function:: void data_sync_barrier(void)

   Data synchronization barrier.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void data_sync_barrier(void) {
          asm volatile("fence rw, rw" ::: "memory");
      }
   
   **Semantics:**
   
   * Ensures all operations complete before continuing
   * Stronger than data_memory_barrier (waits for completion)
   * Use before signaling or after receiving signals
   
   **Use cases:**
   
   * Before notifying device that data is ready
   * After receiving device interrupt
   * Synchronization points in protocols
   
   **Example:**
   
   .. code-block:: c
   
      // Setup complete, synchronize before notifying
      data_sync_barrier();
      
      // Ring doorbell
      virtio_notify_queue(queue_id);

Instruction Barrier
~~~~~~~~~~~~~~~~~~~

.. c:function:: void instruction_barrier(void)

   Instruction fetch barrier.
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void instruction_barrier(void) {
          asm volatile("fence.i" ::: "memory");
      }
   
   **Semantics:**
   
   * Ensures instruction fetch ordering
   * Use after modifying code in memory
   * Flushes instruction cache
   
   **Use cases:**
   
   * JIT compilation
   * Dynamic code loading
   * Self-modifying code
   * After copying executable code
   
   **Example:**
   
   .. code-block:: c
   
      // Copy executable code to memory
      memcpy(code_buffer, jit_compiled_code, code_size);
      
      instruction_barrier();  // Flush I-cache
      
      // Execute JIT code
      ((void (*)(void))code_buffer)();

Compiler Barrier
~~~~~~~~~~~~~~~~

.. c:function:: void compiler_barrier(void)

   Compiler optimization barrier (no hardware fence).
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void compiler_barrier(void) {
          asm volatile("" ::: "memory");
      }
   
   **Semantics:**
   
   * Prevents compiler from reordering memory accesses
   * Does NOT emit any instructions
   * Does NOT prevent CPU reordering
   * Only affects compile-time optimization
   
   **Use cases:**
   
   * Volatile variable access ordering
   * Preventing dead code elimination
   * Timing-sensitive code
   
   **Example:**
   
   .. code-block:: c
   
      // Ensure loop isn't optimized away
      for (int i = 0; i < DELAY_COUNT; i++) {
          compiler_barrier();  // Prevent optimization
      }

Helper Functions
----------------

Register Access Helpers
~~~~~~~~~~~~~~~~~~~~~~~

.. c:function:: uint32_t read32_barrier(volatile uint32_t *addr)

   Read 32-bit register with barrier.
   
   :param addr: Pointer to memory-mapped register
   :return: Register value
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline uint32_t read32_barrier(volatile uint32_t *addr) {
          uint32_t value = *addr;
          read_barrier();
          return value;
      }
   
   **Behavior:**
   
   1. Read register value
   2. Execute read barrier
   3. Return value
   
   **Ensures:** Subsequent reads happen after this read
   
   **Example:**
   
   .. code-block:: c
   
      // Read status register with ordering
      uint32_t status = read32_barrier(&device->status);
      if (status & STATUS_ERROR) {
          uint32_t error_code = read32_barrier(&device->error);
      }

.. c:function:: void write32_barrier(volatile uint32_t *addr, uint32_t value)

   Write 32-bit register with barrier.
   
   :param addr: Pointer to memory-mapped register
   :param value: Value to write
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void write32_barrier(volatile uint32_t *addr, uint32_t value) {
          write_barrier();
          *addr = value;
      }
   
   **Behavior:**
   
   1. Execute write barrier
   2. Write value to register
   
   **Ensures:** Previous writes complete before this write
   
   **Example:**
   
   .. code-block:: c
   
      // Write command register with ordering
      write32_barrier(&device->data, data_value);
      write32_barrier(&device->command, CMD_START);

.. c:function:: void rmw32_barrier(volatile uint32_t *addr, uint32_t mask, uint32_t value)

   Read-modify-write register with barriers.
   
   :param addr: Pointer to memory-mapped register
   :param mask: Bits to modify
   :param value: New value for masked bits
   
   **Implementation:**
   
   .. code-block:: c
   
      static inline void rmw32_barrier(volatile uint32_t *addr, uint32_t mask, uint32_t value) {
          memory_barrier();
          uint32_t old = *addr;
          *addr = (old & ~mask) | (value & mask);
          memory_barrier();
      }
   
   **Behavior:**
   
   1. Full barrier
   2. Read current value
   3. Modify masked bits
   4. Write new value
   5. Full barrier
   
   **Ensures:** Atomic read-modify-write with ordering
   
   **Example:**
   
   .. code-block:: c
   
      // Set bits 4-7 to 0b1010, preserve others
      rmw32_barrier(&device->control, 0xF0, 0xA0);

Usage Patterns
--------------

Device Driver I/O
~~~~~~~~~~~~~~~~~

Standard pattern for device register access:

.. code-block:: c

   // 1. Setup: Write device registers
   write32_barrier(&dev->config, config_value);
   write32_barrier(&dev->address, dma_phys_addr);
   write32_barrier(&dev->length, transfer_size);
   
   // 2. Synchronize: Ensure setup complete
   data_sync_barrier();
   
   // 3. Trigger: Start operation
   write32_barrier(&dev->command, CMD_START);
   
   // 4. Wait: Poll for completion
   while (!(read32_barrier(&dev->status) & STATUS_DONE))
       ;
   
   // 5. Synchronize: Ensure completion visible
   data_sync_barrier();
   
   // 6. Read: Access result
   uint32_t result = read32_barrier(&dev->result);

DMA Setup
~~~~~~~~~

Pattern for DMA descriptor setup:

.. code-block:: c

   // Fill descriptor
   desc->addr = dma_phys_addr(buffer);
   desc->length = buffer_size;
   desc->flags = DESC_READABLE | DESC_WRITABLE;
   
   // Ensure descriptor writes complete
   write_barrier();
   
   // Make descriptor available to device
   avail_ring->ring[idx] = desc_idx;
   avail_ring->idx++;
   
   // Ensure ring update visible
   memory_barrier();
   
   // Notify device
   virtio_write_reg(VIRTIO_QUEUE_NOTIFY, queue_id);

DMA Completion
~~~~~~~~~~~~~~

Pattern for reading DMA results:

.. code-block:: c

   // Wait for device to mark descriptor used
   while (used_ring->idx == last_seen_idx) {
       compiler_barrier();  // Prevent optimization
   }
   
   // Ensure used ring read complete
   read_barrier();
   
   // Get descriptor index
   uint16_t desc_idx = used_ring->ring[idx].id;
   uint32_t len = used_ring->ring[idx].len;
   
   // Ensure descriptor read before buffer access
   read_barrier();
   
   // Access DMA buffer
   uint8_t *buffer = dma_virt_addr(regions[desc_idx]);
   process_data(buffer, len);

Multi-Core Synchronization
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pattern for shared data between cores:

.. code-block:: c

   // Writer core:
   shared->data1 = value1;
   shared->data2 = value2;
   memory_barrier();
   shared->ready = 1;  // Signal to reader
   
   // Reader core:
   while (!shared->ready) {
       compiler_barrier();
   }
   memory_barrier();
   data1 = shared->data1;  // Guaranteed to see writes
   data2 = shared->data2;

Performance Considerations
--------------------------

Barrier Cost
~~~~~~~~~~~~

Memory barriers have performance cost:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50
   
   * - Barrier Type
     - Relative Cost
     - Notes
   * - ``compiler_barrier()``
     - Free
     - No instructions emitted
   * - ``write_barrier()``
     - Low
     - ``fence w,w`` - only stalls writes
   * - ``read_barrier()``
     - Low
     - ``fence r,r`` - only stalls reads
   * - ``memory_barrier()``
     - Medium
     - ``fence rw,rw`` - stalls all memory ops
   * - ``data_sync_barrier()``
     - Medium
     - Same as memory_barrier on RISC-V
   * - ``instruction_barrier()``
     - High
     - ``fence.i`` - flushes pipeline

**Guidelines:**

* Use most specific barrier needed (write/read vs full)
* Avoid barriers in hot loops
* Batch operations between barriers when possible
* Compiler barrier is free - use liberally for volatile access

Over-Fencing
~~~~~~~~~~~~

Too many barriers hurt performance:

.. code-block:: c

   // BAD: Excessive barriers
   for (int i = 0; i < 1000; i++) {
       buffer[i] = data[i];
       memory_barrier();  // 1000 barriers!
   }
   
   // GOOD: Single barrier after loop
   for (int i = 0; i < 1000; i++) {
       buffer[i] = data[i];
   }
   memory_barrier();  // 1 barrier

Under-Fencing
~~~~~~~~~~~~~

Too few barriers cause bugs:

.. code-block:: c

   // BAD: Missing barrier - race condition
   desc->ready = 0;
   desc->addr = new_addr;
   desc->ready = 1;  // Device might see ready=1 before addr written!
   
   // GOOD: Barrier ensures ordering
   desc->ready = 0;
   desc->addr = new_addr;
   memory_barrier();
   desc->ready = 1;

Debugging Memory Ordering Bugs
-------------------------------

Symptoms
~~~~~~~~

Memory ordering bugs have distinctive symptoms:

* **Intermittent failures**: Work sometimes, fail randomly
* **Hardware-dependent**: Fail on some platforms, work on others
* **Timing-dependent**: Fail under load, work when debugging
* **Data corruption**: Wrong data but no crash

Common Bugs
~~~~~~~~~~~

1. **Missing Barrier**
   
   .. code-block:: c
   
      // Setup DMA
      desc->addr = buffer;
      desc->len = size;
      *DEVICE_START = 1;  // BUG: desc might not be visible
   
   **Fix:** Add barrier before starting device

2. **Wrong Barrier Type**
   
   .. code-block:: c
   
      // Write descriptors
      for (int i = 0; i < N; i++) {
          desc[i] = ...;
      }
      read_barrier();  // BUG: should be write_barrier
      notify_device();
   
   **Fix:** Use write_barrier for write ordering

3. **Barrier in Wrong Place**
   
   .. code-block:: c
   
      memory_barrier();
      desc->ready = 1;   // Barrier doesn't help here
      desc->addr = buf;  // These can still reorder!
   
   **Fix:** Barrier must be BETWEEN the operations

Debugging Techniques
~~~~~~~~~~~~~~~~~~~~

1. **Add Full Barriers Everywhere**
   
   Start with ``memory_barrier()`` everywhere, then optimize:
   
   .. code-block:: c
   
      desc->addr = buffer;
      memory_barrier();  // Full barrier
      desc->len = size;
      memory_barrier();  // Full barrier
      notify();
   
   If this fixes the bug, you have an ordering issue.

2. **Disable Compiler Optimization**
   
   .. code-block:: bash
   
      # Build with -O0
      make CFLAGS="-O0"
   
   If bug disappears, likely compiler reordering issue.

3. **Add Delays**
   
   .. code-block:: c
   
      desc->ready = 1;
      for (volatile int i = 0; i < 10000; i++);  // Delay
      notify();
   
   If delay fixes bug, you have a timing/ordering issue.

4. **Check on Real Hardware**
   
   QEMU may not accurately model weak memory ordering.
   Test on actual RISC-V hardware if available.

Implementation Notes
--------------------

Inline Assembly Syntax
~~~~~~~~~~~~~~~~~~~~~~

RISC-V barriers use GCC extended inline assembly:

.. code-block:: c

   asm volatile("fence rw, rw" ::: "memory");
   
   // Breakdown:
   // asm volatile - inline assembly, don't optimize away
   // "fence rw, rw" - RISC-V fence instruction
   // ::: - no input/output operands
   // "memory" - clobber list, tells compiler memory changed

**The "memory" clobber:**

* Tells compiler all memory could change
* Prevents reordering across the asm
* Critical for barrier semantics

Portability
~~~~~~~~~~~

Barriers are architecture-specific. ThunderOS defines them in ``include/arch/barrier.h``:

.. code-block:: text

   include/
   └── arch/
       └── barrier.h          # RISC-V implementation
   
   Future:
       └── x86/
           └── barrier.h      # x86 implementation (mfence, etc.)

Drivers should use barrier functions, not raw ``asm volatile``.

Volatile Qualifier
~~~~~~~~~~~~~~~~~~

``volatile`` is NOT a substitute for barriers:

.. code-block:: c

   volatile int *device_reg;
   
   // Compiler won't reorder DEVICE_REG accesses with each other
   *device_reg = 1;
   *device_reg = 2;
   
   // But CAN reorder with non-volatile accesses:
   buffer[0] = data;
   *device_reg = CMD_START;  // Might see uninitialized buffer!

**Solution:** Use both ``volatile`` and barriers:

.. code-block:: c

   buffer[0] = data;
   memory_barrier();
   *device_reg = CMD_START;  // Now safe

Testing
-------

The memory barrier API is tested in ``tests/test_memory_mgmt.c``:

.. code-block:: c

   // Test 6: Memory Barriers
   memory_barrier();
   write_barrier();
   read_barrier();
   io_barrier();
   data_memory_barrier();
   data_sync_barrier();
   compiler_barrier();
   // All barriers execute without crash

   // Test 7: Barrier Helper Functions
   volatile uint32_t *ptr = (volatile uint32_t *)dma_virt_addr(region);
   write32_barrier(ptr, 0xDEADBEEF);
   uint32_t value = read32_barrier(ptr);
   assert(value == 0xDEADBEEF);

**Functional testing** is limited - barriers don't change single-threaded behavior. Real testing requires:

* Multi-core stress tests
* Device driver integration tests
* Real hardware validation

References
----------

RISC-V Specifications
~~~~~~~~~~~~~~~~~~~~~~

* **RISC-V Volume 1, Unprivileged Spec, Chapter 14**: Memory Ordering
* **RISC-V Volume 2, Privileged Spec, Section 3.7**: Memory Fences

Linux Kernel
~~~~~~~~~~~~

* ``Documentation/memory-barriers.txt``
* ``arch/riscv/include/asm/barrier.h``
* ``include/asm-generic/barrier.h``

Academic Papers
~~~~~~~~~~~~~~~

* *A Primer on Memory Consistency* (Sorin et al.)
* *RISC-V Weak Memory Ordering* (RISC-V Foundation)

Related Documentation
---------------------

* :doc:`dma` - DMA allocator (uses barriers for device I/O)
* :doc:`paging` - Virtual memory (TLB fencing)
* :doc:`../development/device_drivers` - Writing device drivers

See Also
--------

* VirtIO spec section on memory barriers
* ARM memory ordering documentation (similar weak model)
* x86 memory model (strong ordering, different barriers)
