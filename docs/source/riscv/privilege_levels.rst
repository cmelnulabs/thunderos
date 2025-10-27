Privilege Levels
================

RISC-V defines a flexible privilege level system that allows efficient isolation between different software layers.

Overview
--------

RISC-V supports up to four privilege levels:

.. code-block:: text

   ┌────────────────────────────────────────┐
   │  Level 3: Machine Mode (M-mode)        │  ← Highest privilege
   │  - Firmware (OpenSBI)                  │
   │  - Direct hardware access              │
   │  - Always present                      │
   └────────────────────────────────────────┘
                    │
   ┌────────────────────────────────────────┐
   │  Level 2: Hypervisor Mode (H-mode)     │
   │  - Optional                            │
   │  - VM hypervisor                       │
   └────────────────────────────────────────┘
                    │
   ┌────────────────────────────────────────┐
   │  Level 1: Supervisor Mode (S-mode)     │  ← ThunderOS runs here
   │  - Operating system kernel             │
   │  - Virtual memory management           │
   │  - Delegated trap handling             │
   └────────────────────────────────────────┘
                    │
   ┌────────────────────────────────────────┐
   │  Level 0: User Mode (U-mode)           │  ← Lowest privilege
   │  - Application code                    │
   │  - Restricted access                   │
   └────────────────────────────────────────┘

Privilege Level Encoding
------------------------

The current privilege level is stored in machine-mode CSRs:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Mode
     - Encoding
     - Typical Use
   * - U-mode
     - 00
     - User applications
   * - S-mode
     - 01
     - OS kernel (ThunderOS)
   * - H-mode
     - 10
     - Hypervisor (optional)
   * - M-mode
     - 11
     - Firmware, boot ROM

Machine Mode (M-mode)
---------------------

Characteristics
~~~~~~~~~~~~~~~

* **Mandatory**: All RISC-V implementations must support M-mode
* **Full access**: Can access all hardware and CSRs
* **Trap handling**: Handles all traps not delegated to lower levels
* **Boot code**: First code executed after reset

Responsibilities
~~~~~~~~~~~~~~~~

1. **Hardware initialization**
   
   * Configure memory controllers
   * Setup interrupt controllers (PLIC, CLINT)
   * Initialize peripherals

2. **Firmware services**
   
   * Provide SBI (Supervisor Binary Interface) to S-mode
   * Handle timer interrupts
   * Manage power state transitions

3. **Security**
   
   * Physical Memory Protection (PMP) configuration
   * Secure boot implementation
   * Trusted execution environment

In ThunderOS
~~~~~~~~~~~~

* **OpenSBI firmware** runs in M-mode
* Provides SBI version 0.2 services
* Delegates most traps to S-mode
* Handles timer programming via SBI calls

.. code-block:: c

   // S-mode code calls M-mode via ecall
   static int sbi_set_timer(uint64_t stime_value) {
       register unsigned long a0 asm("a0") = stime_value;
       register unsigned long a7 asm("a7") = 0;  // SBI_SET_TIMER
       
       asm volatile("ecall"  // Trap to M-mode
           : "+r"(a0)
           : "r"(a7)
           : "memory");
       
       return a0;
   }

Supervisor Mode (S-mode)
------------------------

Characteristics
~~~~~~~~~~~~~~~

* **OS kernel mode**: Designed for operating system kernels
* **Virtual memory**: Full control over page tables
* **Delegated traps**: Handles traps delegated from M-mode
* **Limited hardware**: Cannot access some M-mode features

Responsibilities
~~~~~~~~~~~~~~~~

1. **Process management**
   
   * Task scheduling
   * Context switching
   * Inter-process communication

2. **Memory management**
   
   * Page table management (SV39/SV48/SV57)
   * Virtual to physical address translation
   * Memory protection

3. **Device drivers**
   
   * Manage I/O devices
   * Handle device interrupts
   * DMA coordination

4. **System calls**
   
   * Provide interface to user mode
   * Validate user requests
   * Enforce security policies

In ThunderOS
~~~~~~~~~~~~

* **Main execution mode** for the kernel
* Uses SBI for M-mode services (timer, IPI, etc.)
* Handles timer interrupts via delegated traps
* Will implement virtual memory (SV39) in future

Available Resources
~~~~~~~~~~~~~~~~~~~

S-mode can access:

* **CSRs**: ``sstatus``, ``sie``, ``stvec``, ``sscratch``, ``sepc``, ``scause``, ``stval``, ``sip``, ``satp``
* **Instructions**: All base instructions, CSR access, ``sret``, ``sfence.vma``
* **Memory**: Only through virtual addresses (when paging enabled)
* **Interrupts**: Timer, software, and external (if delegated)

Cannot access:

* M-mode CSRs (``mstatus``, ``mie``, etc.)
* Physical memory directly (when paging enabled)
* Some hardware features reserved for M-mode
* ``mret`` instruction

Hypervisor Mode (H-mode)
-------------------------

Characteristics
~~~~~~~~~~~~~~~

* **Optional extension**: Not present in all implementations
* **Virtualization**: Allows running multiple guest OSes
* **Two-level address translation**: Guest virtual → guest physical → host physical

Use Cases
~~~~~~~~~

* Running multiple guest operating systems
* Cloud computing platforms
* Secure partitioning

**Note**: ThunderOS doesn't currently use H-mode.

User Mode (U-mode)
------------------

Characteristics
~~~~~~~~~~~~~~~

* **Lowest privilege**: Most restricted access
* **Application code**: Where user programs run
* **Protected**: Cannot directly access hardware or privileged CSRs
* **System calls**: Must use ``ecall`` to request kernel services

Restrictions
~~~~~~~~~~~~

* Cannot access privileged CSRs
* Cannot execute privileged instructions (``sret``, ``mret``, ``wfi``, ``sfence.vma``)
* Memory access controlled by page tables
* All I/O must go through kernel

In ThunderOS
~~~~~~~~~~~~

* **Future implementation**: Not yet supported
* Will run user applications
* Will use page tables for memory protection
* Will provide system call interface via ``ecall``

Privilege Transitions
----------------------

Increasing Privilege
~~~~~~~~~~~~~~~~~~~~

Privilege level increases (U→S→M) through **traps**:

1. **Exception**: Synchronous event (illegal instruction, page fault, ecall)
2. **Interrupt**: Asynchronous event (timer, external device)

.. code-block:: text

   User Application (U-mode)
         │
         │ ecall (system call)
         ↓
   ┌─────────────────┐
   │  Trap to S-mode │
   └─────────────────┘
         │
         ↓
   Kernel Handler (S-mode)
         │
         │ Need M-mode service? ecall (SBI call)
         ↓
   ┌─────────────────┐
   │  Trap to M-mode │
   └─────────────────┘
         │
         ↓
   Firmware (M-mode)

When a trap occurs:

1. Hardware saves current PC to ``xepc`` (``sepc`` or ``mepc``)
2. Hardware sets privilege to trap handler level
3. Hardware sets cause in ``xcause``
4. Hardware jumps to address in ``xtvec``
5. Trap handler executes
6. Handler uses ``xret`` to return

Decreasing Privilege
~~~~~~~~~~~~~~~~~~~~~

Privilege level decreases through **xret** instructions:

* ``mret``: Return from M-mode trap
* ``sret``: Return from S-mode trap

.. code-block:: asm

   # S-mode trap handler return
   sret  # PC = sepc, privilege = sstatus.SPP, interrupts = sstatus.SPIE

The ``xret`` instruction:

1. Restores PC from ``xepc``
2. Restores privilege level from ``xstatus.xPP``
3. Restores interrupt enable from ``xstatus.xPIE``

Trap Delegation
----------------

M-mode can **delegate** certain traps to S-mode for efficient handling.

Delegation CSRs
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - CSR
     - Description
   * - ``medeleg``
     - Exception delegation (bit N = delegate exception N to S-mode)
   * - ``mideleg``
     - Interrupt delegation (bit N = delegate interrupt N to S-mode)

OpenSBI Configuration
~~~~~~~~~~~~~~~~~~~~~

On QEMU with OpenSBI:

.. code-block:: text

   Boot HART MIDELEG         : 0x0000000000000222
   Boot HART MEDELEG         : 0x000000000000b109

Delegated interrupts (``mideleg`` = 0x222):

* Bit 1: Supervisor software interrupt
* Bit 5: Supervisor timer interrupt  
* Bit 9: Supervisor external interrupt

Delegated exceptions (``medeleg`` = 0xb109):

* Bit 0: Instruction address misaligned
* Bit 3: Breakpoint
* Bit 8: Environment call from U-mode
* Bit 12: Instruction page fault
* Bit 13: Load page fault
* Bit 15: Store/AMO page fault

Without Delegation
~~~~~~~~~~~~~~~~~~

If a trap is **not delegated**:

1. Trap goes to M-mode regardless of where it occurred
2. M-mode handler must manually redirect to S-mode if needed
3. Adds overhead (two trap entries instead of one)

With Delegation
~~~~~~~~~~~~~~~

If a trap **is delegated** to S-mode:

1. S-mode traps go directly to S-mode handler
2. M-mode traps still go to M-mode
3. More efficient (one trap entry)

Example: Timer Interrupt
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Timer fires (hardware event)
         │
         ├─ Delegated? (mideleg bit 5 set)
         │
         ├─ Yes → Jump to stvec (S-mode handler)
         │           │
         │           ↓
         │    clint_handle_timer() ← ThunderOS
         │           │
         │           ↓
         │        sret
         │
         └─ No  → Jump to mtvec (M-mode handler)
                     │
                     ↓
                  OpenSBI redirects to S-mode
                     │
                     ↓
                  sret (back to M-mode)
                     │
                     ↓
                  mret

Memory Access
-------------

Physical Memory Protection (PMP)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

M-mode configures **PMP** to restrict S-mode/U-mode access:

.. code-block:: text

   ┌─────────────────────────────────────┐
   │  M-mode code/data (protected)       │  PMP: M-mode only
   ├─────────────────────────────────────┤
   │  S-mode code/data (kernel)          │  PMP: S-mode + M-mode
   ├─────────────────────────────────────┤
   │  User space (applications)          │  PMP: U-mode + S-mode + M-mode
   └─────────────────────────────────────┘

Virtual Memory
~~~~~~~~~~~~~~

S-mode manages **virtual memory** for U-mode:

* U-mode: Always uses virtual addresses (``satp`` configured by S-mode)
* S-mode: Can use virtual addresses (controlled by ``satp``)
* M-mode: Always uses physical addresses

Instruction Access
------------------

Privileged Instructions
~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 15 30

   * - Instruction
     - U
     - S
     - M
     - Description
   * - ``ecall``
     - ✓
     - ✓
     - ✓
     - Trap to higher privilege
   * - ``ebreak``
     - ✓
     - ✓
     - ✓
     - Breakpoint
   * - ``sret``
     - ✗
     - ✓
     - ✓
     - Return from S-mode
   * - ``mret``
     - ✗
     - ✗
     - ✓
     - Return from M-mode
   * - ``wfi``
     - ✗
     - ✓
     - ✓
     - Wait for interrupt
   * - ``sfence.vma``
     - ✗
     - ✓
     - ✓
     - Flush TLB

CSR Access
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 15 30

   * - CSR Range
     - U
     - S
     - M
     - Examples
   * - 0x000-0x0FF
     - R/W
     - R/W
     - R/W
     - U-mode CSRs
   * - 0x100-0x1FF
     - ✗
     - R/W
     - R/W
     - S-mode CSRs (``sstatus``, ``sie``, etc.)
   * - 0x300-0x3FF
     - ✗
     - ✗
     - R/W
     - M-mode CSRs (``mstatus``, ``mie``, etc.)
   * - 0xC00-0xC1F
     - R
     - R
     - R
     - Counters (``cycle``, ``time``, ``instret``)

Best Practices
--------------

For OS Development
~~~~~~~~~~~~~~~~~~

1. **Run kernel in S-mode**: More portable, better security
2. **Use SBI for M-mode services**: Don't try to access M-mode features directly
3. **Handle delegated traps**: Expect timer/external/page fault interrupts
4. **Implement U-mode eventually**: For running user applications

For Security
~~~~~~~~~~~~

1. **Minimize M-mode code**: Keep firmware small and trusted
2. **Delegate what you can**: Let S-mode handle most traps
3. **Use PMP properly**: Protect M-mode memory from S-mode
4. **Validate transitions**: Check privilege transitions carefully

For Performance
~~~~~~~~~~~~~~~

1. **Delegate interrupts**: Avoid M-mode redirection overhead
2. **Keep traps fast**: Minimize handler latency
3. **Use virtual memory**: Enables process isolation efficiently
4. **Batch SBI calls**: Reduce privilege transitions

Common Pitfalls
---------------

1. **Forgetting delegation**
   
   * Trap goes to M-mode unexpectedly
   * Performance impact from double-trap

2. **Wrong xret instruction**
   
   * Using ``mret`` in S-mode (illegal instruction)
   * Using ``sret`` in U-mode (illegal instruction)

3. **Accessing wrong CSRs**
   
   * S-mode trying to access M-mode CSRs
   * Results in illegal instruction exception

4. **Incorrect privilege encoding**
   
   * Setting wrong ``xstatus.xPP`` value
   * Can return to wrong privilege level

Examples
--------

Checking Current Privilege
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In M-mode:

.. code-block:: asm

   # Read mstatus.MPP (bits 11-12)
   csrr t0, mstatus
   srli t0, t0, 11
   andi t0, t0, 3      # t0 = previous privilege

In S-mode:

.. code-block:: asm

   # Read sstatus.SPP (bit 8)
   csrr t0, sstatus
   srli t0, t0, 8
   andi t0, t0, 1      # t0 = previous privilege (0=U, 1=S)

Transitioning to U-mode
~~~~~~~~~~~~~~~~~~~~~~~~

From S-mode:

.. code-block:: asm

   # Prepare to enter U-mode
   la t0, user_entry       # User code entry point
   csrw sepc, t0           # Set return address
   
   li t1, 0x00000020       # sstatus.SPP = 0 (U-mode), SPIE = 1
   csrw sstatus, t1        # Set status
   
   sret                    # Enter U-mode

SBI Call Example
~~~~~~~~~~~~~~~~

.. code-block:: c

   // S-mode calling M-mode service
   long sbi_console_putchar(int ch) {
       register unsigned long a0 asm("a0") = ch;
       register unsigned long a7 asm("a7") = 1;  // SBI_CONSOLE_PUTCHAR
       
       asm volatile("ecall" : "+r"(a0) : "r"(a7));
       return a0;
   }

See Also
--------

* :doc:`csr_registers` - CSRs for each privilege level
* :doc:`interrupts_exceptions` - Trap handling details
* :doc:`../internals/trap_handler` - ThunderOS trap implementation
* Official RISC-V Privileged Specification
