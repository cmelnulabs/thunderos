Trap Handler
============

The trap handler is ThunderOS's mechanism for responding to exceptions (synchronous events like system calls, page faults, illegal instructions) and interrupts (asynchronous events like timer ticks, external devices). It preserves complete CPU state, identifies the trap cause, dispatches to specialized handlers, and restores execution context.

For RISC-V trap fundamentals, see :doc:`../riscv/interrupts_exceptions`.

Overview
--------

When a trap occurs in ThunderOS, the system transitions through multiple privilege and execution contexts:

1. **Hardware Actions**: RISC-V hart saves ``sepc`` (return address), sets ``scause`` (trap reason) and ``stval`` (fault address), then jumps to ``stvec`` (trap vector address)
2. **Context Detection**: Assembly code detects whether trap originated from user or kernel mode using ``sscratch`` register
3. **State Preservation**: Assembly saves all 32 general-purpose registers plus ``sepc`` and ``sstatus`` to trap frame structure on kernel stack
4. **Handler Dispatch**: C code examines ``scause`` to determine exception vs interrupt, then routes to appropriate handler
5. **State Restoration**: Assembly restores all registers and CSRs from trap frame
6. **Return**: ``sret`` instruction returns to interrupted code with correct privilege level

Architecture
------------

Components
~~~~~~~~~~

ThunderOS's trap handling spans three architectural layers:

.. code-block:: text

   kernel/arch/riscv64/trap_entry.S  - Assembly trap vector, context save/restore, user/kernel mode detection
   kernel/arch/riscv64/core/trap.c   - C trap dispatcher, exception/interrupt handlers
   include/trap.h                    - Trap frame structure (272 bytes), cause constants

Complete Trap Flow
~~~~~~~~~~~~~~~~~~

.. code-block:: text

   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ HARDWARE TRAP ENTRY (RISC-V Hart Actions)                             ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   Trap Condition (exception/interrupt)
         │
         ├─> Save current PC → sepc (exception program counter)
         ├─> Set scause register (trap cause code)
         ├─> Set stval register (fault address or zero)
         ├─> Clear sstatus.SIE (disable interrupts)
         ├─> Set sstatus.SPP to previous privilege (0=User, 1=Supervisor)
         ├─> Set privilege to Supervisor mode
         └─> Jump to address in stvec → trap_vector
   
   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ ASSEMBLY CONTEXT DETECTION (trap_entry.S)                             ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   [trap_vector] entry point
         │
         ├─> csrrw sp, sscratch, sp    (atomically swap sp ↔ sscratch)
         │   
         │   User mode trap:  sscratch=kernel_sp, sp=user_sp
         │                    After swap: sp=kernel_sp, sscratch=user_sp
         │   
         │   Kernel mode trap: sscratch=0, sp=kernel_sp
         │                     After swap: sp=0, sscratch=kernel_sp
         │
         └─> beqz sp, trap_from_kernel  (if sp==0, came from kernel)
   
   ┌───────────────────────────────┐  ┌──────────────────────────────────┐
   │ [trap_from_user]              │  │ [trap_from_kernel]               │
   ├───────────────────────────────┤  ├──────────────────────────────────┤
   │ • csrr t0, sscratch           │  │ • csrrw sp, sscratch, sp         │
   │   (get user sp)               │  │   (restore sp from sscratch)     │
   │ • csrw sscratch, zero         │  │ • addi sp, sp, -272              │
   │   (CRITICAL: mark kernel mode)│  │   (allocate trap frame)          │
   │ • addi sp, sp, -272           │  │ • addi t0, sp, 272               │
   │   (allocate trap frame)       │  │ • sd t0, 8(sp)                   │
   │ • sd t0, 8(sp)                │  │   (save pre-trap sp)             │
   │   (save user sp)              │  │                                  │
   └───────────────┬───────────────┘  └──────────────┬───────────────────┘
                   │                                  │
                   └──────────────┬───────────────────┘
                                  │
   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ ASSEMBLY STATE PRESERVATION (trap_entry.S)                            ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   [save_registers]
         │
         ├─> csrr t0, sepc; sd t0, 248(sp)          (save return address)
         ├─> sd ra, 0(sp)                            (save x1: return addr)
         ├─> sd sp, 8(sp)                            (save x2: already done)
         ├─> sd gp, 16(sp)                           (save x3: global ptr)
         ├─> sd tp, 24(sp)                           (save x4: thread ptr)
         ├─> sd t0-t6, 32-240(sp)                    (save temporaries)
         ├─> sd s0-s11, 56-208(sp)                   (save saved regs)
         ├─> sd a0-a7, 72-128(sp)                    (save arguments)
         ├─> csrr t0, sstatus; sd t0, 256(sp)       (save status register)
         ├─> li t0, (1<<18); csrs sstatus, t0       (set SUM bit for user memory access)
         └─> mv a0, sp; call trap_handler           (pass trap_frame pointer to C)
   
   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ C TRAP DISPATCHER (trap.c)                                            ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   [trap_handler(struct trap_frame *tf)]
         │
         ├─> cause = read_scause()
         │
         ├─> if (cause & INTERRUPT_BIT)  [bit 63 set = interrupt]
         │       └─> handle_interrupt(tf, cause)
         │
         └─> else  [bit 63 clear = exception]
                 └─> handle_exception(tf, cause)
   
   ┌─────────────────────────────────────┐  ┌──────────────────────────────┐
   │ [handle_exception]                  │  │ [handle_interrupt]           │
   ├─────────────────────────────────────┤  ├──────────────────────────────┤
   │ • Check cause code:                 │  │ • cause &= ~INTERRUPT_BIT    │
   │                                     │  │   (clear bit 63)             │
   │   ├─ CAUSE_USER_ECALL (8)           │  │                              │
   │   │   ├─> Get syscall number (a7)   │  │ • switch (cause):            │
   │   │   ├─> syscall_handler(tf, ...)  │  │                              │
   │   │   ├─> Store return in a0        │  │   ├─ IRQ_S_TIMER (5)         │
   │   │   └─> Advance sepc by 4         │  │   │   └─> hal_timer_         │
   │   │                                 │  │   │        handle_interrupt()│
   │   ├─ Page faults (12, 13, 15)       │  │   │        (may call         │
   │   │   └─> User: terminate process   │  │   │         scheduler)       │
   │   │       Kernel: panic & halt      │  │   │                          │
   │   │                                 │  │   ├─ IRQ_S_SOFT (1)          │
   │   └─ Other exceptions               │  │   │   └─> (currently unused) │
   │       └─> User: terminate process   │  │   │                          │
   │           Kernel: panic & halt      │  │   └─ IRQ_S_EXTERNAL (9)      │
   │                                     │  │       └─> handle_external_   │
   │ • signal_deliver_with_frame()       │  │            interrupt() (PLIC)│
   │   (check for pending signals)       │  │                              │
   └─────────────────────────────────────┘  └──────────────────────────────┘
   
   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ ASSEMBLY STATE RESTORATION (trap_entry.S)                             ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   Return from trap_handler to trap_entry.S
         │
         ├─> ld t0, 256(sp)                         (load sstatus)
         ├─> andi t1, t0, (1<<8)                    (check SPP bit)
         └─> beqz t1, restore_to_user               (branch if returning to user)
   
   ┌─────────────────────────────────┐  ┌──────────────────────────────────┐
   │ [restore_to_kernel]             │  │ [restore_to_user]                │
   ├─────────────────────────────────┤  ├──────────────────────────────────┤
   │ • ld t0, 248(sp)                │  │ • ld t0, 248(sp)                 │
   │   csrw sepc, t0                 │  │   csrw sepc, t0                  │
   │   (restore return address)      │  │   (restore return address)       │
   │                                 │  │                                  │
   │ • ld t0, 256(sp)                │  │ • ld t0, 256(sp)                 │
   │   ori t0, t0, (1<<5)            │  │   ori t0, t0, (1<<5)             │
   │   csrw sstatus, t0              │  │   csrw sstatus, t0               │
   │   (restore status, set SPIE=1)  │  │   (restore status, set SPIE=1)   │
   │                                 │  │                                  │
   │ • Restore all 32 registers:     │  │ • Restore all 32 registers:      │
   │   ld ra, 0(sp)                  │  │   ld ra, 0(sp)                   │
   │   ld gp, 16(sp)                 │  │   ld gp, 16(sp)                  │
   │   ... (all x1-x31) ...          │  │   ... (all x1-x31) ...           │
   │                                 │  │                                  │
   │ • ld sp, 8(sp)                  │  │ • addi t6, sp, 272               │
   │   (restore kernel sp)           │  │   csrw sscratch, t6              │
   │                                 │  │   (CRITICAL: save kernel sp top) │
   │ • csrw sscratch, zero           │  │                                  │
   │   (mark kernel mode)            │  │ • ld sp, 8(sp)                   │
   │                                 │  │   (restore user sp)              │
   └─────────────────┬───────────────┘  └──────────────┬───────────────────┘
                     │                                  │
                     └──────────────┬───────────────────┘
                                    │
   ╔═══════════════════════════════════════════════════════════════════════╗
   ║ HARDWARE TRAP EXIT (RISC-V Hart Actions)                              ║
   ╚═══════════════════════════════════════════════════════════════════════╝
   
   sret instruction
         │
         ├─> Restore PC from sepc (jump to sepc address)
         ├─> Restore privilege from sstatus.SPP (0→User, 1→Supervisor)
         ├─> Copy sstatus.SPIE → sstatus.SIE (restore interrupt enable)
         ├─> Set sstatus.SPIE = 1
         └─> Resume interrupted code execution


Detailed Stage Explanations
============================

Stage 1: Hardware Trap Entry
-----------------------------

When a trap condition occurs (exception or interrupt), the RISC-V hardware automatically performs these atomic operations **before** any software code executes:

.. note::
   These operations are performed by the **RISC-V CPU hardware**, not by software. They happen atomically and instantaneously when a trap is detected. No code in any file implements these steps - they are built into the processor's microarchitecture. The ``stvec`` CSR (step 7) is configured by ``trap_init()`` in ``kernel/arch/riscv64/core/trap.c`` to point to ``trap_vector`` in ``kernel/arch/riscv64/trap_entry.S``, where software execution begins after the hardware completes these steps.

**What Happens:**

1. **Save Return Address**: Current PC → ``sepc`` CSR (Supervisor Exception Program Counter)
   
   The hardware saves the address of the instruction where the trap occurred (exceptions) or the next instruction to execute (interrupts). This allows resuming execution after the trap is handled.

2. **Record Cause**: Trap reason → ``scause`` CSR (bit 63: 0=exception, 1=interrupt; bits 0-62: cause code)
   
   The hardware writes a code indicating what caused the trap. Software reads this to determine how to handle the trap. Exception codes include syscalls (8), page faults (12/13/15), illegal instructions (2). Interrupt codes include timer (5), software (1), external (9).

3. **Record Fault Info**: Faulting address or instruction → ``stval`` CSR (Supervisor Trap Value)
   
   For page faults, ``stval`` contains the virtual address that caused the fault. For illegal instruction exceptions, it may contain the instruction itself. For other traps, this may be zero or undefined.

4. **Disable Interrupts**: Clear ``sstatus.SIE`` bit (Supervisor Interrupt Enable) to prevent nested traps
   
   The hardware automatically disables interrupts to prevent a new interrupt from occurring while handling the current trap. This prevents stack corruption and ensures the trap handler runs atomically. Interrupts are re-enabled when ``sret`` executes (via ``SPIE`` → ``SIE`` copy).

5. **Record Previous Privilege**: Current privilege → ``sstatus.SPP`` bit (0=User, 1=Supervisor)
   
   The hardware saves whether the trap came from User mode (SPP=0) or Supervisor mode (SPP=1). This is critical because it determines: (a) which stack to use (user vs kernel), and (b) which privilege level to return to when ``sret`` executes.

6. **Elevate Privilege**: Switch to Supervisor mode
   
   Regardless of where the trap originated, execution always continues in Supervisor mode. This ensures the trap handler has full access to system resources and CSRs. The original privilege level is preserved in ``sstatus.SPP`` for restoration later.

7. **Jump to Handler**: PC → address in ``stvec`` CSR (points to ``trap_vector`` in ThunderOS)
   
   The hardware loads the PC with the address stored in ``stvec``, causing execution to jump to the software trap handler. ThunderOS initializes ``stvec`` during boot to point to the ``trap_vector`` label in ``kernel/arch/riscv64/trap_entry.S``. From this point forward, software is in control.

**Why This Matters:**

* Hardware atomicity ensures no races between trap detection and handler entry
* Saving ``sepc`` allows returning to exact instruction (exceptions) or next instruction (interrupts)
* ``sstatus.SPP`` tells software which privilege level to restore on return
* Disabling interrupts (``SIE=0``) prevents handler corruption from nested traps

**Typical Trap Causes:**

* **Exceptions**: System call (``ecall`` instruction), page fault, illegal instruction, misaligned access
* **Interrupts**: Timer tick, external device, software interrupt

Stage 2: Context Detection (sscratch Mechanism)
------------------------------------------------

ThunderOS uses the ``sscratch`` CSR to distinguish user-mode traps from kernel-mode traps. This register serves as a "mode indicator":

**Convention:**

* **User mode execution**: ``sscratch`` = kernel stack pointer (top of current process's kernel stack)
* **Kernel mode execution**: ``sscratch`` = 0

**Detection Logic:**

.. code-block:: asm

   trap_vector:
       csrrw sp, sscratch, sp    # Atomically swap sp ↔ sscratch
       beqz sp, trap_from_kernel  # If sp now zero, came from kernel

**Case 1: Trap from User Mode**

* **Before swap**: ``sp`` = user stack, ``sscratch`` = kernel stack
* **After swap**: ``sp`` = kernel stack, ``sscratch`` = user stack
* **Result**: Now running on kernel stack with access to user stack pointer

**Case 2: Trap from Kernel Mode**

* **Before swap**: ``sp`` = kernel stack, ``sscratch`` = 0
* **After swap**: ``sp`` = 0, ``sscratch`` = kernel stack
* **Result**: ``sp`` is zero, signaling kernel-mode trap; restore ``sp`` from ``sscratch``

**Why This Design:**

* Single atomic instruction (``csrrw``) eliminates race conditions
* No conditional branching before knowing privilege level
* Preserves both user and kernel stack pointers correctly
* Minimal overhead: one CSR operation to detect mode

**The Nested Trap Problem:**

After entering the trap handler from user mode, ``sscratch`` contains the user's stack pointer. If we leave it this way, a serious problem occurs when a **nested trap** happens (a trap occurring while already handling a trap):

**Scenario Without Proper sscratch Update:**

1. User program executes (``sscratch = kernel_stack_top``)
2. System call trap occurs → enters ``trap_vector``
3. After swap: ``sp = kernel_stack``, ``sscratch = user_stack``
4. **Problem**: ``sscratch`` still contains ``user_stack`` (non-zero value)
5. While handling system call, timer interrupt arrives (nested trap)
6. Nested trap enters ``trap_vector`` again
7. Swap instruction: ``sp ↔ sscratch`` → now ``sp = user_stack``! ❌
8. Code checks ``beqz sp, trap_from_kernel`` → fails (sp not zero)
9. System **incorrectly** thinks this is a user-mode trap
10. Tries to allocate trap frame on **user stack** → security vulnerability and corruption

**Solution - Marking Kernel Mode:**

Immediately after saving the user stack pointer in the trap frame, the code sets ``sscratch = 0``:

.. code-block:: asm

   trap_from_user:
       csrr t0, sscratch      # Get user sp from sscratch
       csrw sscratch, zero    # CRITICAL: Mark we're now in kernel mode
       addi sp, sp, -272      # Allocate trap frame on kernel stack
       sd t0, 8(sp)           # Save user sp to trap frame

Now if a nested trap occurs:

1. Nested trap enters ``trap_vector``
2. Swap: ``sp ↔ sscratch`` → ``sp`` becomes 0 (since ``sscratch = 0``)
3. ``beqz sp, trap_from_kernel`` → **succeeds** ✓
4. System **correctly** identifies this as kernel-mode trap
5. Continues on kernel stack safely

**Summary:**

The ``sscratch`` register acts as a state indicator:

* **sscratch = 0**: "Currently in kernel mode, any trap is nested"
* **sscratch = kernel_stack**: "Currently in user mode, trap needs stack switch"

Setting ``sscratch = 0`` immediately after entering from user mode ensures nested traps are handled correctly.

.. danger::
   **CRITICAL:** After trapping from user mode, ``sscratch`` must be set to 0 before any other trap can occur. Failure to do so causes nested traps to corrupt the user stack.

Stage 3: State Preservation
----------------------------

After determining the trap source, ThunderOS saves complete CPU state to a **trap frame** structure on the kernel stack.

Trap Frame Structure
~~~~~~~~~~~~~~~~~~~~

**Location**: ``include/trap.h``

**Size**: 272 bytes (34 × 8-byte registers)

**Layout**:

.. code-block:: c

   struct trap_frame {
       unsigned long ra;      // Offset 0:   x1  (return address)
       unsigned long sp;      // Offset 8:   x2  (stack pointer)
       unsigned long gp;      // Offset 16:  x3  (global pointer)
       unsigned long tp;      // Offset 24:  x4  (thread pointer)
       unsigned long t0;      // Offset 32:  x5  (temporary)
       unsigned long t1;      // Offset 40:  x6  (temporary)
       unsigned long t2;      // Offset 48:  x7  (temporary)
       unsigned long s0;      // Offset 56:  x8  (saved/frame pointer)
       unsigned long s1;      // Offset 64:  x9  (saved)
       unsigned long a0;      // Offset 72:  x10 (argument/return value)
       unsigned long a1;      // Offset 80:  x11 (argument/return value)
       unsigned long a2;      // Offset 88:  x12 (argument)
       unsigned long a3;      // Offset 96:  x13 (argument)
       unsigned long a4;      // Offset 104: x14 (argument)
       unsigned long a5;      // Offset 112: x15 (argument)
       unsigned long a6;      // Offset 120: x16 (argument)
       unsigned long a7;      // Offset 128: x17 (argument/syscall number)
       unsigned long s2;      // Offset 136: x18 (saved)
       unsigned long s3;      // Offset 144: x19 (saved)
       unsigned long s4;      // Offset 152: x20 (saved)
       unsigned long s5;      // Offset 160: x21 (saved)
       unsigned long s6;      // Offset 168: x22 (saved)
       unsigned long s7;      // Offset 176: x23 (saved)
       unsigned long s8;      // Offset 184: x24 (saved)
       unsigned long s9;      // Offset 192: x25 (saved)
       unsigned long s10;     // Offset 200: x26 (saved)
       unsigned long s11;     // Offset 208: x27 (saved)
       unsigned long t3;      // Offset 216: x28 (temporary)
       unsigned long t4;      // Offset 224: x29 (temporary)
       unsigned long t5;      // Offset 232: x30 (temporary)
       unsigned long t6;      // Offset 240: x31 (temporary)
       unsigned long sepc;    // Offset 248: Supervisor exception PC
       unsigned long sstatus; // Offset 256: Supervisor status register
   };

**Register note**: x0 (zero register) is hardwired to 0 and never saved.

Save Sequence
~~~~~~~~~~~~~

.. code-block:: asm

   save_registers:
       # Save return address (sepc)
       csrr t0, sepc
       sd t0, 248(sp)
       
       # Save all general-purpose registers x1-x31
       sd ra, 0(sp)      # x1
       # sp (x2) already saved during context detection
       sd gp, 16(sp)     # x3
       sd tp, 24(sp)     # x4
       sd t0, 32(sp)     # x5
       # ... (continues for all registers) ...
       sd t6, 240(sp)    # x31
       
       # Save processor status
       csrr t0, sstatus
       sd t0, 256(sp)

**SUM Bit Configuration**:

After saving state, the code sets the SUM (permit Supervisor User Memory access) bit in ``sstatus``:

.. code-block:: asm

   li t0, (1 << 18)
   csrs sstatus, t0

**Why SUM is Critical:**

* System calls often access user-mode memory (e.g., ``read()``, ``write()``, ``execve()``)
* Without SUM=1, kernel gets page faults accessing user buffers
* Must be set **after** saving original ``sstatus`` to preserve user-mode state
* Automatically cleared on ``sret`` return to user mode

**Calling Convention Preservation:**

The trap frame preserves RISC-V calling convention registers:

* **a0-a7**: Function arguments; ``a0`` also holds return values; ``a7`` holds syscall numbers
* **t0-t6**: Temporary registers (caller-saved)
* **s0-s11**: Saved registers (callee-saved)
* **ra**: Return address for function calls

This allows C code to call functions normally, knowing all registers are safely preserved.

Stage 4: C Handler Dispatch
----------------------------

With all state saved, control transfers to C code via ``trap_handler(struct trap_frame *tf)``.

Handler Entry Point
~~~~~~~~~~~~~~~~~~~

**Location**: ``kernel/arch/riscv64/core/trap.c``

**Signature**: ``void trap_handler(struct trap_frame *tf)``

**Parameter**: Pointer to trap frame on kernel stack (``sp`` when entering C code)

Main Dispatcher Logic
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void trap_handler(struct trap_frame *tf) {
       unsigned long cause = read_scause();
       
       if (cause & INTERRUPT_BIT) {
           // Bit 63 set: asynchronous interrupt
           handle_interrupt(tf, cause);
       } else {
           // Bit 63 clear: synchronous exception
           handle_exception(tf, cause);
       }
       
       // Check for pending signals before returning to user mode
       struct process *current = process_current();
       if (current) {
           signal_deliver_with_frame(current, tf);
           
           // If signal stopped process, reschedule
           if (current->state == PROC_STOPPED) {
               schedule();
           }
       }
   }

**Key Mechanism**: The ``scause`` register's bit 63 distinguishes asynchronous (interrupt) from synchronous (exception) traps.

Exception Handling (handle_exception)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Synchronous traps** result directly from instruction execution (page fault, illegal instruction, system call).

**Primary Exception Types:**

1. **User ECALL (cause 8)**: System call from user mode

   * Extract syscall number from ``a7`` register
   * Extract arguments from ``a0-a5`` registers
   * Call ``syscall_handler_with_frame(tf, ...)``
   * Store return value in ``a0``
   * Advance ``sepc`` by 4 bytes (past ``ecall`` instruction)
   * Special case: ``execve`` success modifies trap frame to jump to new program entry point

2. **Page Faults (cause 12/13/15)**: Invalid memory access

   * **User mode**: Print fault details, terminate process via ``process_exit(-1)``
   * **Kernel mode**: Print fault details, halt system (kernel bug)
   * ``stval`` CSR contains faulting virtual address

3. **Other Exceptions** (illegal instruction, misaligned access, breakpoint):

   * **User mode**: Terminate offending process
   * **Kernel mode**: Panic and halt (kernel bug)

**Error Recovery Strategy:**

* User-mode exceptions → isolated: terminate only the offending process
* Kernel-mode exceptions → catastrophic: halt system (no safe recovery)

Interrupt Handling (handle_interrupt)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Asynchronous traps** arrive independent of instruction execution (timer, external device).

**Interrupt Types:**

1. **Timer Interrupt (IRQ 5)**:

   * Calls ``hal_timer_handle_interrupt()``
   * Timer handler may call scheduler for preemptive multitasking
   * Critical for process scheduling and timekeeping

2. **Software Interrupt (IRQ 1)**:

   * Currently unused in ThunderOS
   * Future use: inter-processor interrupts (IPI) in SMP systems

3. **External Interrupt (IRQ 9)**:

   * Routes to ``handle_external_interrupt()``
   * Uses PLIC (Platform-Level Interrupt Controller) to identify device
   * Handles VirtIO devices, UART, GPIO, etc.

**Interrupt Masking:**

* Hardware automatically disables interrupts (``sstatus.SIE=0``) on trap entry
* Prevents nested interrupts from corrupting handler state
* Re-enabled on ``sret`` via ``sstatus.SPIE`` → ``sstatus.SIE`` copy

Signal Delivery
~~~~~~~~~~~~~~~

Before returning to user mode, ``trap_handler()`` checks for pending signals:

.. code-block:: c

   if (current) {
       signal_deliver_with_frame(current, tf);
       
       if (current->state == PROC_STOPPED) {
           schedule();  // Don't return to stopped process
       }
   }

**Why in Trap Handler:**

* Signals deliver at safe points: system call return, interrupt return
* Trap frame provides complete register context for signal handler setup
* Ensures signals deliver promptly (not delayed until next syscall)

Stage 5: State Restoration
---------------------------

After the C handler returns, assembly code restores CPU state and returns to interrupted execution.

Return Path Selection
~~~~~~~~~~~~~~~~~~~~~

The code examines ``sstatus.SPP`` (Supervisor Previous Privilege) bit to determine return destination:

.. code-block:: asm

   ld t0, 256(sp)              # Load sstatus from trap frame
   andi t1, t0, (1 << 8)       # Extract SPP bit (bit 8)
   beqz t1, restore_to_user    # If SPP=0, return to user mode

**Two distinct restoration paths** handle different privilege levels.

Restore to Kernel Mode
~~~~~~~~~~~~~~~~~~~~~~

**Path**: ``restore_to_kernel`` label

**Sequence**:

1. **Restore CSRs**:

   .. code-block:: asm

      ld t0, 248(sp)
      csrw sepc, t0        # Restore return address
      
      ld t0, 256(sp)
      ori t0, t0, (1<<5)   # Set SPIE=1 (re-enable interrupts)
      csrw sstatus, t0     # Restore status register

2. **Restore General-Purpose Registers**: Load all x1-x31 from trap frame

3. **Restore Stack Pointer**:

   .. code-block:: asm

      ld sp, 8(sp)         # Last register to restore

4. **Mark Kernel Mode**:

   .. code-block:: asm

      csrw sscratch, zero  # Ensure sscratch=0 for kernel mode

5. **Return**:

   .. code-block:: asm

      sret                 # Return to kernel code

Restore to User Mode
~~~~~~~~~~~~~~~~~~~~

**Path**: ``restore_to_user`` label

**Sequence**:

1. **Restore CSRs**: Same as kernel mode (``sepc``, ``sstatus`` with SPIE=1)

2. **Restore General-Purpose Registers**: All x1-x31 except ``sp``

3. **Setup Stack Swap for Next Trap** (CRITICAL):

   .. code-block:: asm

      addi t6, sp, 272      # Calculate kernel stack top
      csrw sscratch, t6     # Save kernel sp to sscratch

   **Why**: Next trap from user mode needs kernel stack address in ``sscratch``

4. **Restore User Stack Pointer**:

   .. code-block:: asm

      ld sp, 8(sp)          # Load user sp from trap frame

5. **Return**:

   .. code-block:: asm

      sret                  # Return to user code

**Asymmetry Explanation:**

* Kernel mode: ``sscratch=0`` (mode indicator)
* User mode: ``sscratch=kernel_stack_top`` (enables stack swap on next trap)

SPIE Bit (Re-enabling Interrupts)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Both restoration paths set ``sstatus.SPIE=1`` before writing to ``sstatus``:

.. code-block:: asm

   ori t0, t0, (1 << 5)   # Set SPIE (Supervisor Previous Interrupt Enable)
   csrw sstatus, t0

**Why This Matters:**

* Hardware disabled interrupts on trap entry (``SIE=0``)
* ``sret`` copies ``SPIE`` → ``SIE``, restoring interrupt state
* Without setting ``SPIE=1``, interrupts remain permanently disabled after first trap
* Critical for timer interrupts and preemptive scheduling

Stage 6: Hardware Return (sret)
--------------------------------

The ``sret`` (Supervisor Return) instruction atomically performs final trap exit:

**Actions**:

1. **Restore Program Counter**: ``sepc`` → PC (jump to return address)
2. **Restore Privilege Level**: ``sstatus.SPP`` → current privilege (0→User, 1→Supervisor)
3. **Restore Interrupt Enable**: ``sstatus.SPIE`` → ``sstatus.SIE`` (re-enable interrupts if SPIE=1)
4. **Reset SPIE**: Set ``sstatus.SPIE=1`` (default for next trap)
5. **Resume Execution**: Fetch next instruction from ``sepc``

**Execution Resumes**:

* **Exception**: Resumes at instruction **after** the faulting instruction (system calls advance ``sepc`` by 4)
* **Interrupt**: Resumes at **same** instruction that was interrupted (transparent to interrupted code)

**Privilege Transition**:

* User → Kernel → User: System call or page fault
* Kernel → Kernel: Timer interrupt during kernel execution

Critical Invariants
===================

ThunderOS's trap handling maintains these invariants for correctness:

sscratch Register State
------------------------

**Invariant**: ``sscratch`` **must** accurately reflect current privilege level at all times.

* **User mode execution**: ``sscratch`` = kernel stack pointer
* **Kernel mode execution**: ``sscratch`` = 0
* **User trap handler**: Must set ``sscratch=0`` immediately after saving user sp
* **User return path**: Must restore ``sscratch=kernel_sp`` before ``sret``

**Violation**: Incorrect ``sscratch`` causes nested traps to use wrong stack → stack corruption

Interrupt State (SIE/SPIE Bits)
--------------------------------

**Invariant**: Interrupts disabled during trap handling; restored on return.

* Hardware clears ``SIE`` on trap entry
* Handler runs with interrupts disabled (no nested interrupts)
* Restoration code sets ``SPIE=1`` before ``sret``
* ``sret`` copies ``SPIE`` → ``SIE``, re-enabling interrupts

**Violation**: Forgetting to set ``SPIE=1`` permanently disables interrupts → system freeze

Stack Pointer Management
-------------------------

**Invariant**: Kernel stack used for trap handling; user stack untouched.

* Trap from user: Switch to kernel stack via ``sscratch`` swap
* Trap from kernel: Continue on kernel stack
* Trap frame allocated on kernel stack (272 bytes)
* Return restores original stack pointer

**Violation**: Using user stack in kernel mode → security vulnerabilities, corruption

Trap Frame Integrity
---------------------

**Invariant**: Trap frame completely preserves pre-trap CPU state.

* All 31 general-purpose registers saved (x1-x31)
* CSRs saved: ``sepc``, ``sstatus``
* Structure size: 272 bytes (34 registers × 8 bytes)
* Modifications to trap frame affect restored state (syscall return values, signal handlers)

**Violation**: Incomplete or corrupted trap frame → unpredictable behavior, crashes

SUM Bit Configuration
----------------------

**Invariant**: SUM bit set during kernel trap handling; cleared on user return.

* Set after saving original ``sstatus`` to trap frame
* Allows kernel to access user-mode memory (syscall parameters, buffers)
* Hardware automatically clears SUM when returning to user mode via ``sret``

**Violation**: Forgetting to set SUM → page faults accessing user memory in syscalls

Register Save Order
-------------------

**Invariant**: Registers saved before being clobbered; ``sp`` restored last.

* ``t0`` used as scratch register after saving to trap frame
* ``sp`` restored last (all other registers loaded first)
* Stack pointer always valid during restoration

**Violation**: Incorrect order → register corruption, stack pointer loss

Initialization
==============

Trap handling initialization occurs early in ``kernel_main()``:

.. code-block:: c

   void trap_init(void) {
       extern void trap_vector(void);
       
       // Set stvec to trap handler entry point
       // Mode: Direct (0) - all traps jump to BASE address
       asm volatile("csrw stvec, %0" :: "r"((unsigned long)trap_vector));
       
       hal_uart_puts("Trap handler initialized\n");
   }

**stvec CSR**:

* **BASE** (bits 63:2): Trap vector base address (must be 4-byte aligned)
* **MODE** (bits 1:0): 
  
  * 0 = Direct: All traps jump to BASE
  * 1 = Vectored: Interrupts jump to BASE + 4×cause (not used in ThunderOS)

**Initialization Order** (from ``kernel_main()``):

1. UART initialization (for early boot messages)
2. Memory management (PMM, paging)
3. **Trap handler** ← ``trap_init()``
4. Timer initialization (enables timer interrupts)
5. Scheduler initialization

**Why This Order:**

* Trap handler must be ready **before** enabling any interrupts
* Timer initialization enables timer interrupts → requires trap handler
* Early initialization allows handling exceptions during boot

Trap Causes Reference
======================

ThunderOS currently handles these trap causes:

Exceptions (Synchronous)
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Code
     - Name
     - ThunderOS Behavior
   * - 0
     - Instruction address misaligned
     - User: terminate; Kernel: panic
   * - 1
     - Instruction access fault
     - User: terminate; Kernel: panic
   * - 2
     - Illegal instruction
     - User: terminate; Kernel: panic
   * - 3
     - Breakpoint
     - User: terminate (no debugger yet); Kernel: panic
   * - 4
     - Load address misaligned
     - User: terminate; Kernel: panic
   * - 5
     - Load access fault
     - User: terminate; Kernel: panic
   * - 6
     - Store/AMO address misaligned
     - User: terminate; Kernel: panic
   * - 7
     - Store/AMO access fault
     - User: terminate; Kernel: panic
   * - 8
     - Environment call from U-mode
     - **System call** - handle and return
   * - 9
     - Environment call from S-mode
     - Kernel: panic (should not occur)
   * - 12
     - Instruction page fault
     - User: terminate; Kernel: panic (TODO: demand paging)
   * - 13
     - Load page fault
     - User: terminate; Kernel: panic (TODO: demand paging)
   * - 15
     - Store/AMO page fault
     - User: terminate; Kernel: panic (TODO: demand paging)

Interrupts (Asynchronous)
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - IRQ
     - Name
     - ThunderOS Behavior
   * - 1
     - Supervisor software interrupt
     - Currently unused (reserved for IPI)
   * - 5
     - Supervisor timer interrupt
     - **Handle timer tick** - call scheduler, update timekeeping
   * - 9
     - Supervisor external interrupt
     - **Handle device interrupt** - dispatch via PLIC to VirtIO, UART, etc.

For complete RISC-V cause code reference, see :doc:`../riscv/interrupts_exceptions`.

CSR Usage Summary
=================

ThunderOS trap handling uses these supervisor-mode CSRs:

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - CSR
     - Usage
   * - **stvec**
     - Trap vector address - points to ``trap_vector`` in trap_entry.S
   * - **scause**
     - Trap cause identifier - bit 63: interrupt flag; bits 0-62: cause code
   * - **sepc**
     - Exception program counter - saved by hardware; return address for ``sret``
   * - **stval**
     - Trap value - faulting address (page faults) or instruction (illegal instruction)
   * - **sstatus**
     - Supervisor status - SPP (previous privilege), SIE (interrupt enable), SPIE (previous interrupt enable), SUM (supervisor user memory access)
   * - **sscratch**
     - Supervisor scratch - kernel stack pointer (user mode) or 0 (kernel mode)
   * - **sie**
     - Supervisor interrupt enable - individual interrupt enable bits (timer, software, external)
   * - **sip**
     - Supervisor interrupt pending - shows pending interrupts

For detailed CSR documentation, see :doc:`../riscv/csr_registers`.

Nested Traps
============

**Current Behavior**: ThunderOS **disables nested traps** during trap handling.

**Mechanism**:

* Hardware clears ``sstatus.SIE`` on trap entry
* Interrupts remain disabled throughout handler execution
* ``sret`` restores ``SIE`` from ``SPIE``

**Implications**:

* **Safe**: No interrupt can corrupt trap handler state
* **Simple**: No need for interrupt handler reentrancy
* **Limited Responsiveness**: High-priority interrupts delayed until trap handler completes
* **Bounded Latency**: Trap handler execution time determines interrupt latency

**Future Enhancement**: Nested interrupt support with priority levels

* Set ``sstatus.SIE=1`` in interrupt handlers (not exception handlers)
* Use priority mechanism to prevent low-priority interrupts from preempting high-priority handlers
* Requires careful stack management and reentrancy design
* Improves real-time responsiveness for critical interrupts

**Nested Trap Scenarios** (currently prevented):

1. **Exception during exception handling**: Indicates kernel bug (double fault) → panic
2. **Interrupt during exception handling**: Delayed until exception handler completes
3. **Interrupt during interrupt handling**: Delayed until first interrupt completes

Testing and Debugging
======================

Trap Handler Test Suite
------------------------

**Location**: ``tests/unit/test_*.c``

**Test Coverage**:

* Trap frame structure size and alignment
* CSR read/write operations
* Exception cause codes
* Interrupt routing
* User/kernel mode detection
* Stack switching logic

**Running Tests**:

.. code-block:: bash

   cd /workspace/tests
   ./scripts/run_all_tests.sh

For comprehensive testing documentation, see :doc:`testing_framework`.

Debugging Traps with GDB
-------------------------

**Common Debugging Scenarios**:

1. **Unexpected Exception**:

   .. code-block:: gdb

      (gdb) break handle_exception
      (gdb) continue
      (gdb) print /x *tf          # Examine trap frame
      (gdb) x/10i $sepc           # Disassemble at exception PC

2. **Stack Corruption**:

   .. code-block:: gdb

      (gdb) break trap_vector
      (gdb) watch $sp             # Watch stack pointer changes
      (gdb) print /x $sscratch    # Check sscratch value

3. **Interrupt Storm**:

   .. code-block:: gdb

      (gdb) break handle_interrupt
      (gdb) commands
      > silent
      > print cause
      > continue
      > end
      (gdb) continue              # See rapid interrupt causes

**Useful GDB Commands**:

* ``info registers``: Show all general-purpose registers
* ``info reg scause sepc sstatus``: Show supervisor CSRs
* ``backtrace``: Show call stack (kernel mode only)
* ``x/10x $sp``: Examine trap frame on stack

Performance Considerations
===========================

Trap Overhead
-------------

**Typical Trap Latency** (RISC-V RV64 on QEMU virt platform):

* Hardware trap entry: ~10 cycles
* Context save (34 registers): ~68 cycles (34 stores)
* Handler dispatch: ~5-10 cycles
* Context restore: ~68 cycles (34 loads)
* Hardware trap exit (``sret``): ~10 cycles
* **Total**: ~150-170 cycles minimum

**Syscall Overhead**:

* Trap overhead: ~150-170 cycles
* Syscall dispatch: ~10-20 cycles
* Actual syscall work: varies (100s to 1000s of cycles)
* **Total**: System call "tax" is ~10-15% for fast syscalls

**Interrupt Overhead**:

* Trap overhead: ~150-170 cycles
* Interrupt handler: varies by device
* Scheduler decision (timer): ~50-100 cycles
* Context switch (if scheduled): ~200-300 cycles additional

Optimization Opportunities
--------------------------

**Current Design** prioritizes correctness and simplicity. Future optimizations:

1. **Lazy Register Saving**: Save only subset of registers for fast syscalls (not page faults)
2. **Fast Syscall Path**: Dedicated entry point for common syscalls, skip trap frame
3. **Interrupt Coalescing**: Batch multiple interrupts in single handler invocation
4. **Per-CPU Trap Stacks**: Eliminate stack switching overhead in SMP systems (future)

**Trade-offs**:

* Complexity vs. performance
* Code maintainability vs. cycle count
* Security (complete state save) vs. speed (partial save)

Future Enhancements
===================

Planned improvements to ThunderOS trap handling:

Exception Recovery
------------------

**Current**: All kernel-mode exceptions cause panic and halt

**Future**: Graceful recovery for recoverable exceptions

* **Demand Paging**: Handle page faults by loading pages from disk/swap
* **Copy-on-Write**: Handle write faults by duplicating shared pages
* **User Page Faults**: Improved error messages and core dumps

Nested Interrupt Support
-------------------------

**Current**: Interrupts disabled during all trap handling

**Future**: Prioritized nested interrupts

* Enable interrupts in interrupt handlers (not exception handlers)
* Priority mechanism prevents low-priority interrupts from preempting high-priority
* Improved real-time responsiveness for critical devices

User Mode Trap Handling
------------------------

**Current**: Exceptions in user mode terminate process

**Future**: User-mode exception handlers (signal handlers)

* Deliver SIGSEGV, SIGILL, SIGFPE to user processes
* Allow user-mode recovery from exceptions
* Support debugger attachment and breakpoints

Performance Counters
--------------------

**Future**: Track trap statistics

* Trap frequency by cause
* Trap latency measurements
* Interrupt response time
* Syscall profiling

**Use Cases**:

* Performance tuning
* Identifying hot paths
* Detecting interrupt storms
* System call usage analysis

See Also
--------

* :doc:`../riscv/interrupts_exceptions` - RISC-V trap mechanism fundamentals
* :doc:`../riscv/csr_registers` - Complete CSR reference
* :doc:`syscalls` - System call implementation details
* :doc:`process_management` - Process context switching
* :doc:`signals` - Signal delivery mechanism
* :doc:`testing_framework` - Trap handler test suite
