.. _internals-user-mode:

User Mode Support
=================

ThunderOS implements full user mode support with privilege level transitions, address space isolation, and secure kernel-user communication. This chapter details the architecture, implementation, and mechanisms behind user mode.

Overview
--------

User mode support provides:

- **Privilege Separation**: Code runs in User (U) or Supervisor (S) mode
- **Address Space Isolation**: Each user process has its own page table
- **Protected Transitions**: Secure privilege transitions via ``sret`` and ``ecall``
- **Stack Switching**: Efficient kernel stack access during traps
- **Memory Protection**: Permission enforcement at page table level

Architecture
------------

Privilege Levels
~~~~~~~~~~~~~~~~

RISC-V defines three privilege levels:

.. code-block:: text

    Machine Mode (M)     - Highest privilege, firmware/bootloader
    Supervisor Mode (S)  - OS kernel runs here
    User Mode (U)        - Applications run here

ThunderOS runs in Supervisor mode with user processes in User mode.

Address Space Layout
~~~~~~~~~~~~~~~~~~~~

The virtual address space is divided between user and kernel:

.. code-block:: text

    Sv39 Virtual Address Space (64-bit)
    ===================================
    
    User Space (Supervisor view: 0x00000000_00000000 - 0x00000000_3FFFFFFF)
    ├─ 0x00010000: User Code Start (USER_CODE_BASE)
    ├─ 0x00020000: User Heap Start (USER_HEAP_START)
    ├─ 0x40000000: Memory-Mapped Region (USER_MMAP_START)
    └─ 0x80000000: User Stack Top (USER_STACK_TOP)
    
    Kernel Space (VPN[2] = 2-511, 0x80000000+)
    ├─ 0x80200000: Kernel Code/Data
    ├─ 0x02000000: CLINT (Timer/IPI)
    └─ 0x10000000: UART (Console I/O)

Page Table Organization
~~~~~~~~~~~~~~~~~~~~~~~

Each user process has a separate root page table:

.. code-block:: text

    User Process Page Table (Sv39, 3 levels)
    ========================================
    
    Level 2 (Root Table)
    ├─ Entry 0-1: User space (initially unmapped)
    │   ├─ Maps to Level 1 tables for user regions
    │   └─ Level 1 maps to Level 0 for 4KB pages
    ├─ Entry 2-511: Kernel space (copied from kernel table)
    │   ├─ Maps kernel code/data regions
    │   ├─ MMIO regions (UART, CLINT)
    │   └─ All accessible only in Supervisor mode

Privilege Mode Control
~~~~~~~~~~~~~~~~~~~~~~

The ``sstatus`` register controls privilege transitions:

.. code-block:: text

    sstatus Bits (relevant to user mode):
    SPP (bit 8)  = Previous Privilege Level
                   0 = User, 1 = Supervisor
    SPIE (bit 5) = Supervisor Previous Interrupt Enable
                   Set on sret to restore interrupt state
    SIE (bit 1)  = Supervisor Interrupt Enable
                   Interrupt enable in supervisor mode

Process State
~~~~~~~~~~~~~

User processes maintain state through a trap frame:

.. code-block:: c

    struct trap_frame {
        unsigned long ra, sp, gp, tp;        // General purpose
        unsigned long t0, t1, t2;            // Temporaries
        unsigned long s0, s1;                // Saved registers
        unsigned long a0, a1, a2, a3, a4, a5, a6, a7;  // Arguments
        unsigned long s2-s11;                // More saved
        unsigned long t3, t4, t5, t6;        // More temporaries
        unsigned long sepc;     // Exception PC (offset 248)
        unsigned long sstatus;  // Status register (offset 256)
    };

.. note::
   The trap frame is 264 bytes (33 x 8 bytes) and must be 8-byte aligned.

Implementation
--------------

User Page Table Creation
~~~~~~~~~~~~~~~~~~~~~~~~

Function: ``create_user_page_table()``

Creates a new page table for user processes:

.. code-block:: c

    page_table_t *create_user_page_table(void) {
        // Allocate root page table
        page_table_t *user_pt = alloc_page_table();
        
        // Copy kernel mappings (entries 2-511)
        for (int i = 2; i < PT_ENTRIES; i++) {
            user_pt->entries[i] = kernel_page_table.entries[i];
        }
        
        // Leave user space empty (entries 0-1 = 0)
        user_pt->entries[0] = 0;
        user_pt->entries[1] = 0;
        
        return user_pt;
    }

This approach:

1. **Allocates a fresh page table** - New 4KB page from physical memory
2. **Copies kernel mappings** - Entries 2-511 provide kernel access
3. **Leaves user space empty** - Entries 0-1 initially unmapped
4. **Enables kernel access from user** - Via accessible MMIO regions

Code Mapping
~~~~~~~~~~~~

Function: ``map_user_code()``

Maps executable user code into the process address space:

.. code-block:: c

    int map_user_code(page_table_t *page_table, uintptr_t user_vaddr,
                      void *kernel_code, size_t size) {
        // Align to page boundaries
        size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        
        for (size_t i = 0; i < num_pages; i++) {
            // Allocate physical page
            uintptr_t phys_page = pmm_alloc_page();
            
            // Copy code from kernel space
            memcpy((void *)phys_page, kernel_code, PAGE_SIZE);
            
            // Map with PTE_USER_TEXT (R, X, U)
            map_page(page_table, user_vaddr + i*PAGE_SIZE, 
                    phys_page, PTE_USER_TEXT);
        }
        return 0;
    }

Key points:

- **Allocates physical pages** - From PMM (physical memory manager)
- **Copies code** - From kernel space to physical pages
- **Maps with execute permission** - PTE_USER_TEXT = readable + executable + user
- **Not writable** - User code is read-only

Memory Mapping
~~~~~~~~~~~~~~

Function: ``map_user_memory()``

Maps user data regions (stack, heap, etc.):

.. code-block:: c

    int map_user_memory(page_table_t *page_table, uintptr_t user_vaddr,
                        uintptr_t phys_addr, size_t size, int writable) {
        uint64_t flags = writable ? PTE_USER_DATA : PTE_USER_RO;
        
        // For each page in region
        for (size_t i = 0; i < num_pages; i++) {
            uintptr_t phys_page = (phys_addr == 0) ? 
                pmm_alloc_page() : phys_addr + i*PAGE_SIZE;
            
            map_page(page_table, user_vaddr + i*PAGE_SIZE,
                    phys_page, flags);
        }
        return 0;
    }

Features:

- **Can allocate or map** - ``phys_addr=0`` allocates new pages
- **Flexible permissions** - Writable or read-only
- **Multiple regions** - Stack, heap, data all supported

Page Table Switching
~~~~~~~~~~~~~~~~~~~~

Function: ``switch_page_table()``

Changes the active page table for a process:

.. code-block:: c

    void switch_page_table(page_table_t *page_table) {
        uintptr_t root_pa = (uintptr_t)page_table;
        
        // Build satp value
        uint64_t satp = SATP_MODE_SV39 | (root_pa >> SATP_PPN_SHIFT);
        
        // Write to satp register
        asm volatile("csrw satp, %0" :: "r"(satp));
        
        // Flush TLB
        tlb_flush(0);
    }

The ``satp`` register format:

.. code-block:: text

    [63:60] MODE    = 8 for Sv39
    [59:44] ASID    = 0 (not used)
    [43:0]  PPN     = Physical page number of root table

Trap Handling with Stack Switching
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

File: ``kernel/arch/riscv64/trap_entry.S``

The trap handler uses ``sscratch`` for efficient stack switching:

**Entry Sequence:**

.. code-block:: asm

    trap_vector:
        # Swap sp and sscratch
        csrrw sp, sscratch, sp
        
        # Check if we came from user or kernel
        beqz sp, trap_from_kernel
    
    trap_from_user:
        # sp now has kernel stack, sscratch has user stack
        addi sp, sp, -272          # Make room for trap frame
        csrr t0, sscratch          # Get user sp
        sd t0, 16(sp)              # Save in trap frame
        j save_registers
    
    trap_from_kernel:
        # Restore sp and continue
        csrrw sp, sscratch, sp
        addi sp, sp, -272
        # Save kernel sp normally

**Why this works:**

1. **User mode**: ``sscratch`` contains kernel stack pointer
2. **Kernel mode**: ``sscratch`` is zero
3. **Swap**: If ``sp==0`` after swap, came from kernel
4. **Efficiency**: Only one CSR instruction at entry

Return to User Mode
~~~~~~~~~~~~~~~~~~~

File: ``kernel/arch/riscv64/user_return.S``

Pure assembly function returns to user mode safely:

.. code-block:: asm

    user_return:
        mv t6, a0              # a0 = trap_frame pointer
        
        # Restore sepc (exception PC)
        ld t0, 0(t6)
        csrw sepc, t0
        
        # Restore sstatus
        ld t0, 256(t6)
        csrw sstatus, t0
        
        # Restore all registers from trap frame
        ld ra, 8(t6)
        ld sp, 16(t6)
        ld gp, 24(t6)
        # ... restore all 32 registers ...
        
        # Return to user mode (SPP bit in sstatus controls this)
        sret

**Why pure assembly:**

- **No compiler interference** - Compiler doesn't generate prologue/epilogue
- **Exact register restoration** - No unexpected clobbers
- **Clean sret execution** - Register values correct when sret fires

User Process Creation
~~~~~~~~~~~~~~~~~~~~~

Function: ``process_create_user()``

Creates a new user process:

.. code-block:: c

    struct process *process_create_user(const char *name, 
                                        void *user_code, 
                                        size_t code_size) {
        // 1. Allocate process structure
        struct process *proc = alloc_process();
        
        // 2. Create user page table
        proc->page_table = create_user_page_table();
        
        // 3. Map user code
        map_user_code(proc->page_table, USER_CODE_BASE, 
                     user_code, code_size);
        
        // 4. Allocate and map user stack
        uintptr_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
        map_user_memory(proc->page_table, stack_base, 0, 
                       USER_STACK_SIZE, 1);
        
        // 5. Setup trap frame for user mode entry
        proc->trap_frame->sepc = USER_CODE_BASE;
        proc->trap_frame->sp = USER_STACK_TOP;
        proc->trap_frame->sstatus = (1 << 5);  // SPIE=1, SPP=0
        
        // 6. Add to scheduler
        scheduler_enqueue(proc);
        
        return proc;
    }

Entry Wrapper
~~~~~~~~~~~~~

Function: ``user_mode_entry_wrapper()``

Bridges kernel context to user mode:

.. code-block:: c

    void user_mode_entry_wrapper(void) {
        struct process *proc = process_current();
        
        // Switch to user page table
        switch_page_table(proc->page_table);
        
        // Setup sscratch with kernel stack
        uintptr_t kernel_sp = proc->kernel_stack + KERNEL_STACK_SIZE;
        asm volatile("csrw sscratch, %0" :: "r"(kernel_sp));
        
        // Enter user mode (never returns)
        user_return(proc->trap_frame);
    }

This function:

1. **Switches page table** - CPU now uses user page table
2. **Sets sscratch** - Prepares for next trap
3. **Calls user_return** - Enters user mode

Transitions
-----------

User to Supervisor (Trap Entry)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

    User Mode Code
    ↓
    Trap Event (exception/interrupt/ecall)
    ↓
    Hardware Actions:
    ├─ Save sepc = PC of trap instruction
    ├─ Set scause = trap cause
    ├─ Set SPP = 0 (remember we were in user mode)
    ├─ Jump to trap_vector
    └─ Still in user mode!
    ↓
    trap_vector (trap_entry.S)
    ├─ csrrw sp, sscratch, sp   # Swap to kernel stack
    ├─ Save all registers
    ├─ Save sepc and sstatus
    └─ Call trap_handler (C code)
    ↓
    Kernel Code Executes
    ├─ Handle exception/interrupt
    ├─ May modify trap_frame->sstatus
    └─ Return

Supervisor to User (Trap Exit)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

    trap_handler Returns
    ↓
    trap_entry.S Restore Path
    ├─ Check SPP in sstatus (trap_frame)
    ├─ If SPP=0, returning to user
    │  ├─ Set sscratch = kernel stack
    │  └─ Restore registers from trap frame
    └─ If SPP=1, returning to kernel
       ├─ Set sscratch = 0
       └─ Restore registers from trap frame
    ↓
    sret Instruction
    ├─ Load PC from sepc
    ├─ Restore interrupt state from SPIE
    ├─ Set privilege = SPP
    └─ Jump to user/kernel code

Page Table Switching
~~~~~~~~~~~~~~~~~~~~

When scheduler switches to a user process:

.. code-block:: text

    schedule() selects new process
    ↓
    context_switch_asm() to new kernel stack
    ↓
    user_mode_entry_wrapper()
    ├─ Verify process is user mode
    ├─ switch_page_table()
    │  ├─ Write new page table to satp
    │  ├─ Flush TLB
    │  └─ CPU now uses user page table
    ├─ csrw sscratch, kernel_sp
    ├─ user_return()
    │  ├─ Restore all registers
    │  ├─ csrw sepc, user_pc
    │  ├─ csrw sstatus, user_status
    │  └─ sret  # Enter user mode!

Permission Model
----------------

Page Table Entry Flags
~~~~~~~~~~~~~~~~~~~~~~

Each PTE has permission bits:

.. code-block:: c

    #define PTE_V     (1 << 0)   // Valid
    #define PTE_R     (1 << 1)   // Readable
    #define PTE_W     (1 << 2)   // Writable
    #define PTE_X     (1 << 3)   // Executable
    #define PTE_U     (1 << 4)   // User accessible
    #define PTE_G     (1 << 5)   // Global
    #define PTE_A     (1 << 6)   // Accessed
    #define PTE_D     (1 << 7)   // Dirty

Permission Combinations
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #define PTE_USER_TEXT  (PTE_R | PTE_X | PTE_U)      // User code
    #define PTE_USER_DATA  (PTE_R | PTE_W | PTE_U)      // User data
    #define PTE_USER_RO    (PTE_R | PTE_U)              // User read-only
    #define PTE_KERNEL_DATA (PTE_R | PTE_W | PTE_X)     // Kernel

Kernel code has no ``U`` bit, so user mode cannot access it even if mapped.

Memory Protection
~~~~~~~~~~~~~~~~~

The MMU enforces permissions:

.. code-block:: text

    User Mode Access Attempt
    ↓
    MMU Checks:
    ├─ Is page in TLB?
    ├─ Is PTE valid?
    ├─ Does PTE have U bit?
    ├─ Does access match permissions?
    │  ├─ Read: check R bit
    │  ├─ Write: check W bit
    │  └─ Execute: check X bit
    ├─ If all OK: grant access
    └─ Else: page fault exception
         ↓
         CPU triggers load/store/instruction page fault
         ↓
         Trap handler receives fault

Testing
-------

Test Code
~~~~~~~~~

User mode is tested with a minimal user program:

.. code-block:: c

    // User program (runs in user mode)
    uint8_t user_code[] = {
        // lui sp, 0x80000      # Load stack pointer
        // addi sp, sp, 0
        // ecall                # System call to exit
    };

Testing validates:

1. **User process creation** - Process structure initialized correctly
2. **Code mapping** - User code at 0x10000 with X permission
3. **Stack allocation** - User stack allocated and mapped
4. **Page table isolation** - Each process has own page table
5. **Process state** - Listed in process table with READY status

Future Enhancements
-------------------

System Calls
~~~~~~~~~~~~

Implement ECALL handling:

.. code-block:: c

    #define SYS_EXIT    1
    #define SYS_WRITE   2
    #define SYS_READ    3
    #define SYS_GETPID  4

When user code calls ``ecall``:

1. Hardware: Trigger CAUSE_USER_ECALL exception
2. Trap handler: Detect ``scause == 8``
3. Syscall dispatcher: Read ``a7`` register for syscall number
4. Execute syscall: Modify trap frame as needed
5. Return: ``sret`` continues user code

Signal Handling
~~~~~~~~~~~~~~~

Support signals for user processes:

- **SIGTERM**: Terminate process
- **SIGSTOP**: Stop process
- **SIGCONT**: Continue process
- **SIGUSR1/2**: User-defined signals

Signals interrupt user code and call signal handler.

Process Forking
~~~~~~~~~~~~~~~

Implement ``fork()`` system call:

- Copy parent's page table (copy-on-write optimization)
- Duplicate process state
- Child starts at same ``sepc`` with return value in ``a0``

Memory Management
~~~~~~~~~~~~~~~~~

Enhance user memory:

- **Heap management**: ``brk()`` and ``sbrk()`` syscalls
- **Page faults**: Handle lazy allocation for stack growth
- **COW (Copy-on-Write)**: Share pages between processes

Related Documentation
---------------------

- :ref:`internals-paging` - Page table implementation
- :ref:`internals-process-management` - Process structures
- :ref:`internals-trap-handler` - Trap handling details
- :ref:`internals-memory-layout` - Virtual memory layout
