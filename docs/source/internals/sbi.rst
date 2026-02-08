SBI (Supervisor Binary Interface)
===================================

Overview
--------

The Supervisor Binary Interface (SBI) provides a standardized interface between the operating system running in Supervisor mode (S-mode) and the machine-mode firmware. It allows the OS to request privileged operations like system shutdown, reboot, and hardware management without directly accessing machine-mode CSRs.

ThunderOS implements SBI support for system control operations, with multiple fallback mechanisms to ensure reliable shutdown and reboot functionality across different firmware implementations.

Architecture
------------

SBI Call Mechanism
~~~~~~~~~~~~~~~~~~

SBI calls use the RISC-V ``ecall`` instruction to trap from S-mode to M-mode:

.. code-block:: text

   ┌─────────────────────────────┐
   │  ThunderOS (S-mode)         │
   │  - Prepare arguments        │
   │  - Set extension ID (a7)    │
   │  - Set function ID (a6)     │
   │  - Execute ECALL            │
   └──────────────┬──────────────┘
                  │
                  │ ECALL (S→M trap)
                  v
   ┌─────────────────────────────┐
   │  SBI Firmware (M-mode)      │
   │  - Handle ECALL trap        │
   │  - Execute privileged op    │
   │  - Return result            │
   └──────────────┬──────────────┘
                  │
                  │ SRET (M→S return)
                  v
   ┌─────────────────────────────┐
   │  ThunderOS (S-mode)         │
   │  - Receive result           │
   │  - Continue execution       │
   └─────────────────────────────┘

SBI Extensions
~~~~~~~~~~~~~~

ThunderOS supports multiple SBI extensions:

**SRST (System Reset Extension)**

* **Extension ID**: ``0x53525354`` (ASCII "SRST")
* **Function ID**: 0
* **Purpose**: Modern shutdown and reboot interface
* **Supported Operations**:
  
  - ``SBI_SRST_RESET_TYPE_SHUTDOWN`` (0): Power off
  - ``SBI_SRST_RESET_TYPE_COLD_REBOOT`` (1): Cold reboot
  - ``SBI_SRST_RESET_TYPE_WARM_REBOOT`` (2): Warm reboot

**Legacy Shutdown Extension**

* **Extension ID**: ``0x08``
* **Function ID**: 0
* **Purpose**: Older firmware shutdown interface
* **Status**: Deprecated but supported as fallback

Return Structure
~~~~~~~~~~~~~~~~

SBI calls return a structured result:

.. code-block:: c

   typedef struct {
       long error;   // 0 on success, negative error code on failure
       long value;   // Return value (if applicable)
   } sbi_ret_t;

**Common Error Codes:**

.. code-block:: c

   SBI_SUCCESS                =  0
   SBI_ERR_FAILED             = -1
   SBI_ERR_NOT_SUPPORTED      = -2
   SBI_ERR_INVALID_PARAM      = -3
   SBI_ERR_DENIED             = -4
   SBI_ERR_INVALID_ADDRESS    = -5
   SBI_ERR_ALREADY_AVAILABLE  = -6

Implementation
--------------

Core Functions
~~~~~~~~~~~~~~

sbi_ecall
^^^^^^^^^

Low-level wrapper for SBI calls:

.. code-block:: c

   sbi_ret_t sbi_ecall(long ext, long fid, long arg0, long arg1,
                       long arg2, long arg3, long arg4, long arg5);

**Parameters:**

* ``ext``: Extension ID (e.g., ``SBI_EXT_SRST``)
* ``fid``: Function ID within extension
* ``arg0-arg5``: Function-specific arguments

**Returns:**

* ``sbi_ret_t`` structure with error code and value

**Assembly Implementation:**

.. code-block:: c

   asm volatile(
       "ecall"
       : "+r"(a0), "+r"(a1)
       : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
       : "memory"
   );

sbi_shutdown
^^^^^^^^^^^^

Gracefully shutdown the system:

.. code-block:: c

   void sbi_shutdown(void);

**Shutdown Method Priority:**

1. **QEMU Test Device** (0x100000)
   
   * Most reliable on QEMU
   * Write ``0x5555`` to device for success exit
   * Requires kernel page table for MMIO access
   
2. **SBI SRST Extension**
   
   * Standard modern interface
   * Type: ``SBI_SRST_RESET_TYPE_SHUTDOWN``
   * Reason: ``SBI_SRST_RESET_REASON_NONE``
   
3. **Legacy SBI Shutdown**
   
   * Fallback for older firmware
   * Extension ID: ``0x08``
   
4. **WFI Loop**
   
   * Last resort if all methods fail
   * Enters infinite wait-for-interrupt loop

**Implementation:**

.. code-block:: c

   void sbi_shutdown(void)
   {
       hal_uart_puts("\n[SBI] Initiating system shutdown...\n");
       
       /* Try QEMU test device first */
       switch_to_kernel_page_table();
       volatile uint32_t *test_dev = (volatile uint32_t *)QEMU_TEST_DEVICE_ADDR;
       *test_dev = QEMU_TEST_DEVICE_EXIT_SUCCESS;
       
       /* Try SBI SRST extension */
       sbi_ecall(SBI_EXT_SRST, 0,
                 SBI_SRST_RESET_TYPE_SHUTDOWN,
                 SBI_SRST_RESET_REASON_NONE,
                 0, 0, 0, 0);
       
       /* Try legacy shutdown */
       sbi_ecall(SBI_EXT_LEGACY_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
       
       /* Last resort: halt CPU */
       while (1) {
           asm volatile("wfi");
       }
   }

sbi_reboot
^^^^^^^^^^

Reboot the system:

.. code-block:: c

   void sbi_reboot(void);

**Reboot Method Priority:**

1. **QEMU Test Device** (0x100000)
   
   * Write ``0x7777`` for system reset
   
2. **SBI SRST Extension**
   
   * Type: ``SBI_SRST_RESET_TYPE_COLD_REBOOT``
   
3. **WFI Loop**
   
   * Last resort

**Implementation:**

.. code-block:: c

   void sbi_reboot(void)
   {
       hal_uart_puts("\n[SBI] Initiating system reboot...\n");
       
       /* Try QEMU test device */
       switch_to_kernel_page_table();
       volatile uint32_t *test_dev = (volatile uint32_t *)QEMU_TEST_DEVICE_ADDR;
       *test_dev = QEMU_TEST_DEVICE_RESET;
       
       /* Try SBI SRST extension */
       sbi_ecall(SBI_EXT_SRST, 0,
                 SBI_SRST_RESET_TYPE_COLD_REBOOT,
                 SBI_SRST_RESET_REASON_NONE,
                 0, 0, 0, 0);
       
       /* Last resort: halt CPU */
       while (1) {
           asm volatile("wfi");
       }
   }

QEMU Test Device
----------------

Overview
~~~~~~~~

The QEMU test device is a simple MMIO device available in QEMU's ``virt`` machine. It provides a reliable mechanism for system exit and reset.

**Device Address:**

.. code-block:: c

   #define QEMU_TEST_DEVICE_ADDR  0x100000

**Control Values:**

.. code-block:: c

   #define QEMU_TEST_DEVICE_EXIT_SUCCESS  0x5555  // Exit with success
   #define QEMU_TEST_DEVICE_EXIT_FAILURE  0x3333  // Exit with failure
   #define QEMU_TEST_DEVICE_RESET         0x7777  // System reset

Memory Mapping
~~~~~~~~~~~~~~

The QEMU test device must be mapped in the kernel's page table before access:

.. code-block:: c

   // In paging_init()
   if (map_page(&kernel_page_table, QEMU_TEST_DEVICE_ADDR,
                QEMU_TEST_DEVICE_ADDR, PTE_KERNEL_DATA) != 0) {
       hal_uart_puts("Failed to map QEMU test device\n");
       return;
   }

**Important:** When accessing the test device from a syscall context (which may be running with a process page table), we must switch to the kernel page table first:

.. code-block:: c

   /* Switch to kernel page table to access MMIO device */
   switch_to_kernel_page_table();
   
   volatile uint32_t *test_dev = (volatile uint32_t *)QEMU_TEST_DEVICE_ADDR;
   *test_dev = QEMU_TEST_DEVICE_EXIT_SUCCESS;

This is necessary because:

1. Each process has its own page table for memory isolation
2. The test device is only mapped in the kernel page table
3. Attempting to access unmapped MMIO causes a page fault

Usage
~~~~~

**Shutdown:**

.. code-block:: c

   switch_to_kernel_page_table();
   *(volatile uint32_t *)QEMU_TEST_DEVICE_ADDR = QEMU_TEST_DEVICE_EXIT_SUCCESS;

**Reboot:**

.. code-block:: c

   switch_to_kernel_page_table();
   *(volatile uint32_t *)QEMU_TEST_DEVICE_ADDR = QEMU_TEST_DEVICE_RESET;

**Exit with Error:**

.. code-block:: c

   switch_to_kernel_page_table();
   *(volatile uint32_t *)QEMU_TEST_DEVICE_ADDR = QEMU_TEST_DEVICE_EXIT_FAILURE;

Constants
---------

All SBI and QEMU device constants are defined in ``include/arch/sbi.h``:

.. code-block:: c

   /* SBI Extension IDs */
   #define SBI_EXT_SRST              0x53525354
   #define SBI_EXT_LEGACY_SHUTDOWN   0x08
   
   /* SRST Function Parameters */
   #define SBI_SRST_RESET_TYPE_SHUTDOWN      0
   #define SBI_SRST_RESET_TYPE_COLD_REBOOT   1
   #define SBI_SRST_RESET_TYPE_WARM_REBOOT   2
   
   #define SBI_SRST_RESET_REASON_NONE        0
   #define SBI_SRST_RESET_REASON_SYSTEM_FAILURE  1
   
   /* QEMU Test Device */
   #define QEMU_TEST_DEVICE_ADDR         0x100000
   #define QEMU_TEST_DEVICE_EXIT_SUCCESS 0x5555
   #define QEMU_TEST_DEVICE_EXIT_FAILURE 0x3333
   #define QEMU_TEST_DEVICE_RESET        0x7777
   
   /* SBI Error Codes */
   #define SBI_SUCCESS                0
   #define SBI_ERR_FAILED            -1
   #define SBI_ERR_NOT_SUPPORTED     -2
   #define SBI_ERR_INVALID_PARAM     -3

**Design Principle:** No magic numbers - all addresses and control values use named constants.

Testing
-------

QEMU Testing
~~~~~~~~~~~~

**Poweroff Test:**

.. code-block:: bash

   ush> poweroff
   
   =====================================
     System Poweroff Requested
   =====================================
   
   [SBI] Initiating system shutdown...
   
   # QEMU exits cleanly

**Reboot Test:**

.. code-block:: bash

   ush> reboot
   
   =====================================
     System Reboot Requested
   =====================================
   
   [SBI] Initiating system reboot...
   
   # QEMU restarts, kernel boots again

**Expected Behavior:**

* No page faults
* No timeout required
* Clean exit/restart

Debugging
~~~~~~~~~

If shutdown/reboot fails:

1. **Check page table mapping:**
   
   .. code-block:: text
   
      Initializing virtual memory (Sv39)...
      Mapping QEMU test device  ← Should appear in boot log

2. **Verify kernel page table switch:**
   
   .. code-block:: c
   
      // In sbi_shutdown/sbi_reboot
      switch_to_kernel_page_table();  // Must be called before device access

3. **Check for page faults:**
   
   .. code-block:: text
   
      !!! KERNEL EXCEPTION !!!
      Cause: Store/AMO page fault
      stval:  0x0000000000100000  ← Test device address not mapped

4. **Test SBI support:**
   
   Try each method individually to identify which works on your platform.

Files
-----

* ``include/arch/sbi.h`` - SBI constants, structures, function prototypes
* ``kernel/arch/riscv64/drivers/sbi.c`` - SBI implementation
* ``kernel/mm/paging.c`` - QEMU test device mapping
* ``kernel/core/syscall.c`` - Syscall handlers (SYS_POWEROFF, SYS_REBOOT)
* ``userland/system/poweroff.c`` - Poweroff command
* ``userland/system/reboot.c`` - Reboot command

See Also
--------

* :doc:`syscalls` - System call interface (SYS_POWEROFF, SYS_REBOOT)
* :doc:`paging` - Virtual memory management
* `RISC-V SBI Specification <https://github.com/riscv-non-isa/riscv-sbi-doc>`_ - Official SBI documentation
