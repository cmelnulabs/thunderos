API Reference
=============

This document describes the public APIs available in ThunderOS v0.1.0.

Hardware Abstraction Layer (HAL)
---------------------------------

UART Driver
~~~~~~~~~~~

.. c:function:: void hal_uart_init(void)

   Initialize the UART hardware.
   
   Architecture-specific implementation (RISC-V: NS16550A).
   
   :Example:
   
   .. code-block:: c
   
      void kernel_main(void) {
          hal_uart_init();
          // UART ready to use
      }

.. c:function:: void hal_uart_putc(char c)

   Write a single character to UART.
   
   :param c: Character to transmit
   
   **Blocking:** Yes, waits for transmitter to be ready.
   
   :Example:
   
   .. code-block:: c
   
      hal_uart_putc('H');
      hal_uart_putc('i');
      hal_uart_putc('\n');

.. c:function:: void hal_uart_puts(const char *s)

   Write a null-terminated string to UART.
   
   :param s: Null-terminated string to transmit
   
   **Note:** Automatically converts ``\n`` to ``\r\n`` for proper terminal display.
   
   **Blocking:** Yes, waits for each character to transmit.
   
   :Example:
   
   .. code-block:: c
   
      hal_uart_puts("Hello, ThunderOS!\n");
      hal_uart_puts("[OK] System initialized\n");

.. c:function:: int hal_uart_write(const char *buffer, unsigned int count)

   Write a buffer of bytes to UART.
   
   :param buffer: Buffer to transmit
   :param count: Number of bytes to write
   :return: Number of bytes written
   :rtype: int
   
   **Note:** No newline conversion.
   
   :Example:
   
   .. code-block:: c
   
      char data[] = {0x01, 0x02, 0x03};
      hal_uart_write(data, 3);

.. c:function:: char hal_uart_getc(void)

   Read a single character from UART.
   
   :return: Character received from UART
   :rtype: char
   
   **Blocking:** Yes, waits indefinitely for input.
   
   :Example:
   
   .. code-block:: c
   
      hal_uart_puts("Press any key: ");
      char c = hal_uart_getc();
      hal_uart_putc(c);

Timer Driver
~~~~~~~~~~~~

.. c:function:: void hal_timer_init(unsigned long interval_us)

   Initialize timer hardware and start periodic interrupts.
   
   :param interval_us: Timer interrupt interval in microseconds
   
   :Example:
   
   .. code-block:: c
   
      // 100ms timer interrupts
      hal_timer_init(100000);

.. c:function:: unsigned long hal_timer_get_ticks(void)

   Get the current tick count.
   
   :return: Number of timer interrupts since initialization
   :rtype: unsigned long
   
   :Example:
   
   .. code-block:: c
   
      unsigned long start = hal_timer_get_ticks();
      do_work();
      unsigned long elapsed = hal_timer_get_ticks() - start;

.. c:function:: void hal_timer_set_next(unsigned long interval_us)

   Schedule the next timer interrupt.
   
   :param interval_us: Microseconds until next interrupt
   
   :Example:
   
   .. code-block:: c
   
      hal_timer_set_next(50000);  // 50ms

.. c:function:: void hal_timer_handle_interrupt(void)

   Handle timer interrupt (called by trap handler).
   
   Increments tick counter and schedules next interrupt.

Memory Management
-----------------

Physical Memory Manager (PMM)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. c:function:: void pmm_init(uintptr_t mem_start, size_t mem_size)

   Initialize the physical memory manager.
   
   :param mem_start: Start of usable physical memory (after kernel)
   :param mem_size: Total size of usable memory in bytes
   
   :Example:
   
   .. code-block:: c
   
      extern char _kernel_end[];
      pmm_init((uintptr_t)_kernel_end, 128 * 1024 * 1024);

.. c:function:: uintptr_t pmm_alloc_page(void)

   Allocate a single 4KB physical page.
   
   :return: Physical address of allocated page, or 0 if out of memory
   :rtype: uintptr_t
   
   :Example:
   
   .. code-block:: c
   
      uintptr_t page = pmm_alloc_page();
      if (page == 0) {
          kernel_panic("Out of memory");
      }

.. c:function:: uintptr_t pmm_alloc_pages(size_t num_pages)

   Allocate multiple contiguous physical pages.
   
   :param num_pages: Number of contiguous pages to allocate
   :return: Physical address of first page, or 0 on failure
   :rtype: uintptr_t
   
   :Example:
   
   .. code-block:: c
   
      // Allocate 16KB (4 pages)
      uintptr_t pages = pmm_alloc_pages(4);

.. c:function:: void pmm_free_page(uintptr_t page_addr)

   Free a previously allocated physical page.
   
   :param page_addr: Physical address of page (must be page-aligned)
   
   :Example:
   
   .. code-block:: c
   
      pmm_free_page(page);

.. c:function:: void pmm_free_pages(uintptr_t page_addr, size_t num_pages)

   Free multiple contiguous physical pages.
   
   :param page_addr: Physical address of first page
   :param num_pages: Number of pages to free
   
   :Example:
   
   .. code-block:: c
   
      pmm_free_pages(pages, 4);

.. c:function:: void pmm_get_stats(size_t *total_pages, size_t *free_pages)

   Get memory statistics.
   
   :param total_pages: Output - total number of pages managed
   :param free_pages: Output - number of free pages available
   
   :Example:
   
   .. code-block:: c
   
      size_t total, free;
      pmm_get_stats(&total, &free);
      hal_uart_puts("Memory: ");
      kprint_dec(free * 4);
      hal_uart_puts(" KB free\n");

Kernel Heap (kmalloc)
~~~~~~~~~~~~~~~~~~~~~~

.. c:function:: void *kmalloc(size_t size)

   Allocate kernel memory.
   
   :param size: Number of bytes to allocate
   :return: Pointer to allocated memory, or NULL on failure
   :rtype: void*
   
   :Example:
   
   .. code-block:: c
   
      char *buffer = kmalloc(1024);
      if (buffer) {
          // Use buffer
          kfree(buffer);
      }

.. c:function:: void kfree(void *ptr)

   Free kernel memory.
   
   :param ptr: Pointer to memory allocated by kmalloc()
   
   :Example:
   
   .. code-block:: c
   
      void *data = kmalloc(256);
      // ... use data ...
      kfree(data);

.. c:function:: void *kmalloc_aligned(size_t size, size_t align)

   Allocate aligned kernel memory.
   
   :param size: Number of bytes to allocate
   :param align: Alignment requirement (must be power of 2)
   :return: Pointer to aligned memory, or NULL on failure
   :rtype: void*
   
   :Example:
   
   .. code-block:: c
   
      // Allocate page-aligned memory
      void *page = kmalloc_aligned(4096, 4096);

Virtual Memory (Paging)
~~~~~~~~~~~~~~~~~~~~~~~~

.. c:function:: void paging_init(uintptr_t kernel_start, uintptr_t kernel_end)

   Initialize virtual memory system with Sv39 paging.
   
   :param kernel_start: Physical address of kernel start
   :param kernel_end: Physical address of kernel end
   
   :Example:
   
   .. code-block:: c
   
      extern char _text_start[], _kernel_end[];
      paging_init((uintptr_t)_text_start, (uintptr_t)_kernel_end);

.. c:function:: int map_page(page_table_t *page_table, uintptr_t vaddr, uintptr_t paddr, uint64_t flags)

   Map a virtual address to a physical address.
   
   :param page_table: Root page table (level 2)
   :param vaddr: Virtual address (must be page-aligned)
   :param paddr: Physical address (must be page-aligned)
   :param flags: PTE flags (permissions)
   :return: 0 on success, -1 on failure
   :rtype: int
   
   :Example:
   
   .. code-block:: c
   
      page_table_t *pt = get_kernel_page_table();
      map_page(pt, 0x100000, 0x80000000, PTE_KERNEL_DATA);

.. c:function:: int unmap_page(page_table_t *page_table, uintptr_t vaddr)

   Unmap a virtual address.
   
   :param page_table: Root page table
   :param vaddr: Virtual address (must be page-aligned)
   :return: 0 on success, -1 on failure
   :rtype: int

.. c:function:: int virt_to_phys(page_table_t *page_table, uintptr_t vaddr, uintptr_t *paddr)

   Translate virtual address to physical address.
   
   :param page_table: Root page table
   :param vaddr: Virtual address
   :param paddr: Output - physical address
   :return: 0 on success, -1 if not mapped
   :rtype: int

.. c:function:: void tlb_flush(uintptr_t vaddr)

   Flush TLB for a specific virtual address.
   
   :param vaddr: Virtual address to flush (0 = flush all)

.. c:function:: page_table_t *get_kernel_page_table(void)

   Get the kernel's root page table.
   
   :return: Pointer to kernel page table
   :rtype: page_table_t*

.. c:function:: void free_page_table(page_table_t *page_table)

   Free a page table and all its child page tables.
   
   :param page_table: Root page table to free
   
   **WARNING:** Do not call on the kernel page table!

Process Management
------------------

Process API
~~~~~~~~~~~

.. c:function:: void process_init(void)

   Initialize the process management subsystem.
   
   Creates the initial kernel process.

.. c:function:: struct process *process_create(const char *name, void (*entry_point)(void *), void *arg)

   Create a new process.
   
   :param name: Process name
   :param entry_point: Entry point function
   :param arg: Argument to pass to entry point
   :return: Pointer to new process, or NULL on failure
   :rtype: struct process*
   
   :Example:
   
   .. code-block:: c
   
      void user_task(void *arg) {
          while (1) {
              hal_uart_puts("Task running\n");
              process_yield();
          }
      }
      
      struct process *p = process_create("user_task", user_task, NULL);

.. c:function:: void process_exit(int exit_code)

   Exit the current process.
   
   :param exit_code: Exit status code
   
   **Note:** This function never returns.

.. c:function:: struct process *process_current(void)

   Get the currently running process.
   
   :return: Pointer to current process
   :rtype: struct process*

.. c:function:: struct process *process_get(pid_t pid)

   Get process by PID.
   
   :param pid: Process ID
   :return: Pointer to process, or NULL if not found
   :rtype: struct process*

.. c:function:: void process_yield(void)

   Voluntarily yield CPU to another process.
   
   :Example:
   
   .. code-block:: c
   
      while (1) {
          do_work();
          process_yield();  // Let other processes run
      }

.. c:function:: void process_sleep(uint64_t ticks)

   Sleep for a number of timer ticks.
   
   :param ticks: Number of ticks to sleep

.. c:function:: void process_wakeup(struct process *proc)

   Wake up a sleeping process.
   
   :param proc: Process to wake up

.. c:function:: pid_t alloc_pid(void)

   Allocate a new process ID.
   
   :return: New PID, or -1 on failure
   :rtype: pid_t

.. c:function:: void process_free(struct process *proc)

   Free a process structure.
   
   :param proc: Process to free

.. c:function:: void process_dump(void)

   Dump process table for debugging.

Scheduler API
~~~~~~~~~~~~~

.. c:function:: void scheduler_init(void)

   Initialize the round-robin scheduler.

.. c:function:: void schedule(void)

   Schedule next process to run.
   
   Called by timer interrupt for preemptive multitasking.

.. c:function:: void scheduler_enqueue(struct process *proc)

   Add a process to the ready queue.
   
   :param proc: Process to add

.. c:function:: void scheduler_dequeue(struct process *proc)

   Remove a process from the ready queue.
   
   :param proc: Process to remove

.. c:function:: void context_switch(struct process *old, struct process *new)

   Perform context switch between processes.
   
   :param old: Old process (can be NULL)
   :param new: New process

.. c:function:: struct process *scheduler_pick_next(void)

   Get the next process to run.
   
   :return: Next process, or NULL if none available
   :rtype: struct process*

Interrupt Handling
------------------

Trap Handler
~~~~~~~~~~~~

.. c:function:: void trap_init(void)

   Initialize trap/interrupt handling.
   
   Sets up the trap vector and enables interrupts.

.. c:function:: void trap_handler(struct trap_frame *tf)

   Main trap/interrupt handler.
   
   :param tf: Trap frame with saved registers
   
   **Note:** Called by assembly trap_vector routine.

Error Handling
--------------

Panic
~~~~~

.. c:function:: void kernel_panic(const char *message)

   Fatal error handler - halts the system.
   
   :param message: Error message to display
   
   **Note:** This function never returns.
   
   :Example:
   
   .. code-block:: c
   
      if (!critical_resource) {
          kernel_panic("Failed to allocate critical resource");
      }

.. c:macro:: KASSERT(condition, message)

   Kernel assertion macro.
   
   :param condition: Condition to check
   :param message: Message if assertion fails
   
   :Example:
   
   .. code-block:: c
   
      KASSERT(ptr != NULL, "Pointer must not be NULL");

.. c:macro:: BUG(message)

   Mark unreachable code paths.
   
   :param message: Bug description
   
   :Example:
   
   .. code-block:: c
   
      default:
          BUG("Unexpected state");

.. c:macro:: NOT_IMPLEMENTED()

   Mark unimplemented features.
   
   :Example:
   
   .. code-block:: c
   
      void future_feature(void) {
          NOT_IMPLEMENTED();
      }

Constants and Macros
--------------------

Memory Addresses
~~~~~~~~~~~~~~~~

.. c:macro:: UART0_BASE

   Base address of UART0: ``0x10000000``

.. c:macro:: CLINT_BASE

   Base address of CLINT: ``0x02000000``

.. c:macro:: PLIC_BASE

   Base address of PLIC: ``0x0C000000``

.. c:macro:: RAM_START

   Start of RAM: ``0x80000000``

.. c:macro:: KERNEL_BASE

   Kernel load address: ``0x80200000``

Page Flags (Sv39)
~~~~~~~~~~~~~~~~~

.. c:macro:: PTE_V

   Page table entry valid flag

.. c:macro:: PTE_R

   Page table entry readable flag

.. c:macro:: PTE_W

   Page table entry writable flag

.. c:macro:: PTE_X

   Page table entry executable flag

.. c:macro:: PTE_U

   Page table entry user-accessible flag

.. c:macro:: PTE_G

   Page table entry global mapping flag

.. c:macro:: PTE_A

   Page table entry accessed flag

.. c:macro:: PTE_D

   Page table entry dirty flag

Permission Combinations
~~~~~~~~~~~~~~~~~~~~~~~

.. c:macro:: PTE_KERNEL_TEXT

   Kernel code permissions: ``PTE_V | PTE_R | PTE_X``

.. c:macro:: PTE_KERNEL_DATA

   Kernel data permissions: ``PTE_V | PTE_R | PTE_W``

.. c:macro:: PTE_KERNEL_RODATA

   Kernel read-only data: ``PTE_V | PTE_R``

.. c:macro:: PTE_USER_TEXT

   User code permissions: ``PTE_V | PTE_R | PTE_X | PTE_U``

.. c:macro:: PTE_USER_DATA

   User data permissions: ``PTE_V | PTE_R | PTE_W | PTE_U``

Sizes
~~~~~

.. c:macro:: PAGE_SIZE

   Page size: ``4096`` bytes

.. c:macro:: PAGE_SHIFT

   Page shift: ``12`` bits

.. c:macro:: KERNEL_STACK_SIZE

   Kernel stack size per process: ``16384`` bytes (16 KB)

.. c:macro:: USER_STACK_SIZE

   User stack size per process: ``1048576`` bytes (1 MB)

.. c:macro:: MAX_PROCS

   Maximum number of processes: ``64``

.. c:macro:: PROC_NAME_LEN

   Process name maximum length: ``32`` characters

Process States
~~~~~~~~~~~~~~

.. c:macro:: PROC_UNUSED

   Process slot is unused: ``0``

.. c:macro:: PROC_EMBRYO

   Process is being created

.. c:macro:: PROC_READY

   Process is ready to run

.. c:macro:: PROC_RUNNING

   Process is currently running

.. c:macro:: PROC_SLEEPING

   Process is waiting for event

.. c:macro:: PROC_ZOMBIE

   Process has exited but not yet cleaned up

Trap/Exception Causes
~~~~~~~~~~~~~~~~~~~~~~

.. c:macro:: CAUSE_MISALIGNED_FETCH

   Instruction address misaligned: ``0``

.. c:macro:: CAUSE_FETCH_ACCESS

   Instruction access fault: ``1``

.. c:macro:: CAUSE_ILLEGAL_INSTRUCTION

   Illegal instruction: ``2``

.. c:macro:: CAUSE_BREAKPOINT

   Breakpoint: ``3``

.. c:macro:: CAUSE_MISALIGNED_LOAD

   Load address misaligned: ``4``

.. c:macro:: CAUSE_LOAD_ACCESS

   Load access fault: ``5``

.. c:macro:: CAUSE_MISALIGNED_STORE

   Store address misaligned: ``6``

.. c:macro:: CAUSE_STORE_ACCESS

   Store access fault: ``7``

.. c:macro:: CAUSE_USER_ECALL

   Environment call from U-mode: ``8``

.. c:macro:: CAUSE_SUPERVISOR_ECALL

   Environment call from S-mode: ``9``

.. c:macro:: CAUSE_MACHINE_ECALL

   Environment call from M-mode: ``11``

.. c:macro:: CAUSE_FETCH_PAGE_FAULT

   Instruction page fault: ``12``

.. c:macro:: CAUSE_LOAD_PAGE_FAULT

   Load page fault: ``13``

.. c:macro:: CAUSE_STORE_PAGE_FAULT

   Store page fault: ``15``

Interrupt Causes
~~~~~~~~~~~~~~~~

.. c:macro:: IRQ_S_SOFT

   Supervisor software interrupt: ``1``

.. c:macro:: IRQ_S_TIMER

   Supervisor timer interrupt: ``5``

.. c:macro:: IRQ_S_EXTERNAL

   Supervisor external interrupt: ``9``

Data Structures
---------------

Trap Frame
~~~~~~~~~~

.. c:type:: struct trap_frame

   Saved register state during trap/interrupt.
   
   .. c:member:: unsigned long ra
   
      Return address (x1)
   
   .. c:member:: unsigned long sp
   
      Stack pointer (x2)
   
   .. c:member:: unsigned long gp
   
      Global pointer (x3)
   
   .. c:member:: unsigned long tp
   
      Thread pointer (x4)
   
   .. c:member:: unsigned long t0-t6
   
      Temporary registers (x5-x7, x28-x31)
   
   .. c:member:: unsigned long s0-s11
   
      Saved registers (x8-x9, x18-x27)
   
   .. c:member:: unsigned long a0-a7
   
      Argument/return registers (x10-x17)
   
   .. c:member:: unsigned long sepc
   
      Supervisor exception program counter
   
   .. c:member:: unsigned long sstatus
   
      Supervisor status register

Process Context
~~~~~~~~~~~~~~~

.. c:type:: struct context

   Process context for context switching.
   
   .. c:member:: unsigned long ra
   
      Return address
   
   .. c:member:: unsigned long sp
   
      Stack pointer
   
   .. c:member:: unsigned long s0-s11
   
      Callee-saved registers

Process Control Block
~~~~~~~~~~~~~~~~~~~~~~

.. c:type:: struct process

   Process control block (PCB).
   
   .. c:member:: pid_t pid
   
      Process ID
   
   .. c:member:: proc_state_t state
   
      Process state
   
   .. c:member:: char name[PROC_NAME_LEN]
   
      Process name
   
   .. c:member:: page_table_t *page_table
   
      Virtual memory page table
   
   .. c:member:: uintptr_t kernel_stack
   
      Kernel stack base address
   
   .. c:member:: uintptr_t user_stack
   
      User stack base address
   
   .. c:member:: struct context context
   
      Saved kernel context
   
   .. c:member:: struct trap_frame *trap_frame
   
      Saved user context
   
   .. c:member:: uint64_t cpu_time
   
      Total CPU time used (in ticks)
   
   .. c:member:: uint64_t priority
   
      Scheduling priority
   
   .. c:member:: struct process *parent
   
      Parent process
   
   .. c:member:: int exit_code
   
      Exit status code

Page Table
~~~~~~~~~~

.. c:type:: page_table_t

   Sv39 page table (512 entries).
   
   .. c:member:: pte_t entries[512]
   
      Page table entries

.. c:type:: pte_t

   Page table entry (64-bit).

User Mode API
~~~~~~~~~~~~~

.. c:function:: page_table_t *create_user_page_table(void)

   Create a new page table for a user process.
   
   Creates a page table with kernel mappings but empty user space. 
   Each user process must have its own page table for memory isolation.
   
   :return: Pointer to new page table, or NULL on failure
   :rtype: page_table_t*
   
   **Details:**
   
   * Copies kernel entries (2-511) from kernel page table
   * Leaves user space (entries 0-1) unmapped
   * Newly allocated page tables are used for memory isolation
   
   :Example:
   
   .. code-block:: c
   
      page_table_t *user_pt = create_user_page_table();
      if (!user_pt) {
          hal_uart_puts("Failed to create page table\n");
          return NULL;
      }

.. c:function:: int map_user_code(page_table_t *page_table, uintptr_t user_vaddr, void *kernel_code, size_t size)

   Map user code into user address space.
   
   Allocates physical pages, copies code from kernel space, and maps 
   with user-executable permissions.
   
   :param page_table: User process page table
   :param user_vaddr: Virtual address in user space (typically USER_CODE_BASE)
   :param kernel_code: Pointer to code in kernel memory
   :param size: Size of code in bytes
   :return: 0 on success, -1 on failure
   :rtype: int
   
   **Permissions:** R, X, U (executable but not writable)
   
   **Typical Usage:**
   
   .. code-block:: c
   
      if (map_user_code(proc->page_table, USER_CODE_BASE, 
                       user_code, code_size) != 0) {
          hal_uart_puts("Failed to map user code\n");
          return NULL;
      }

.. c:function:: int map_user_memory(page_table_t *page_table, uintptr_t user_vaddr, uintptr_t phys_addr, size_t size, int writable)

   Map user memory (stack, heap, data) into user address space.
   
   Can allocate new physical pages or map existing pages.
   
   :param page_table: User process page table
   :param user_vaddr: Virtual address in user space
   :param phys_addr: Physical address (0 = allocate new pages)
   :param size: Size in bytes
   :param writable: 1 if writable, 0 if read-only
   :return: 0 on success, -1 on failure
   :rtype: int
   
   **Permissions:** 
   
   * If writable: R, W, U (readable and writable)
   * If read-only: R, U (readable only)
   
   **Typical Usage - Stack:**
   
   .. code-block:: c
   
      uintptr_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
      if (map_user_memory(proc->page_table, stack_base, 0, 
                         USER_STACK_SIZE, 1) != 0) {
          hal_uart_puts("Failed to map user stack\n");
          return NULL;
      }

.. c:function:: void switch_page_table(page_table_t *page_table)

   Switch to a different page table.
   
   Updates the ``satp`` register and flushes the TLB. This is called
   when switching between processes.
   
   :param page_table: New page table to switch to
   
   **Details:**
   
   * Writes to ``satp`` CSR (Supervisor Address Translation and Protection)
   * Flushes entire TLB to ensure coherency
   * Used in scheduler and user mode entry wrapper
   
   :Example:
   
   .. code-block:: c
   
      struct process *proc = get_next_process();
      switch_page_table(proc->page_table);
      // CPU now uses proc's page table

.. c:function:: struct process *process_create_user(const char *name, void *user_code, size_t code_size)

   Create a new user-mode process.
   
   Creates a process with its own page table and user address space.
   The user code is mapped at USER_CODE_BASE, and the stack is allocated
   at USER_STACK_TOP.
   
   :param name: Process name (max PROC_NAME_LEN - 1 characters)
   :param user_code: Pointer to user code to execute (in kernel memory)
   :param code_size: Size of user code in bytes
   :return: Pointer to new process, or NULL on failure
   :rtype: struct process*
   
   **Details:**
   
   * Creates separate page table with memory isolation
   * Maps code with executable permissions at USER_CODE_BASE (0x10000)
   * Allocates user stack (1MB) at USER_STACK_TOP (0x80000000)
   * Sets SPP=0 and SPIE=1 in sstatus for user mode entry
   * Adds process to scheduler queue
   
   **Process Layout:**
   
   .. code-block:: text
   
      0x10000      ┌────────────────────┐
                   │   User Code        │  (mapped from user_code)
                   └────────────────────┘
      0x80000000-1MB ┌────────────────────┐
                   │   User Stack       │  (grows downward)
                   │                    │
      0x80000000   └────────────────────┘
   
   :Example:
   
   .. code-block:: c
   
      uint8_t user_code[] = { /* ... */ };
      struct process *proc = process_create_user("myapp", 
                                                 user_code, 
                                                 sizeof(user_code));
      if (!proc) {
          hal_uart_puts("Failed to create user process\n");
          return -1;
      }

.. c:function:: void user_return(struct trap_frame *trap_frame)

   Return to user mode.
   
   Restores all registers from the trap frame and executes ``sret`` to 
   enter user mode. This is a pure assembly function that should never
   return (it enters user mode directly).
   
   :param trap_frame: Pointer to trap frame with register state
   :type trap_frame: struct trap_frame*
   
   **Attributes:** noreturn
   
   **Details:**
   
   * Pure assembly (no C compiler interference)
   * Restores all 32 RISC-V registers from trap frame
   * Sets ``sepc`` to user code entry point
   * Sets ``sstatus`` with SPP=0 for user mode
   * Executes ``sret`` to enter user mode
   
   **Warning:** Must only be called from ``user_mode_entry_wrapper()``
   
   :Example:
   
   .. code-block:: c
   
      // This is called automatically by user_mode_entry_wrapper
      // User code should not call this directly!

Memory Layout Constants
~~~~~~~~~~~~~~~~~~~~~~~

.. c:macro:: USER_CODE_BASE

   User code entry point: ``0x10000`` (64 KB)

.. c:macro:: USER_HEAP_START

   User heap start: ``0x20000`` (128 KB)

.. c:macro:: USER_MMAP_START

   User memory-mapped region: ``0x40000000`` (1 GB)

.. c:macro:: USER_STACK_TOP

   User stack top: ``0x80000000`` (2 GB)

.. c:macro:: USER_STACK_SIZE

   User stack size: ``1048576`` (1 MB, grows downward)

Page Table Entry Permissions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. c:macro:: PTE_USER_TEXT

   User code permissions: R, X, U (readable and executable)

.. c:macro:: PTE_USER_DATA

   User data permissions: R, W, U (readable and writable)

.. c:macro:: PTE_USER_RO

   User read-only permissions: R, U (readable only)

.. c:macro:: PTE_KERNEL_DATA

   Kernel data permissions: R, W, X (all access)

Linker Symbols
--------------

These symbols are defined by the linker script:

.. c:var:: extern char _text_start[]

   Start of ``.text`` section (code)

.. c:var:: extern char _text_end[]

   End of ``.text`` section

.. c:var:: extern char _rodata_start[]

   Start of ``.rodata`` section (read-only data)

.. c:var:: extern char _rodata_end[]

   End of ``.rodata`` section

.. c:var:: extern char _data_start[]

   Start of ``.data`` section (initialized data)

.. c:var:: extern char _data_end[]

   End of ``.data`` section

.. c:var:: extern char _bss_start[]

   Start of ``.bss`` section (uninitialized data)

.. c:var:: extern char _bss_end[]

   End of ``.bss`` section

.. c:var:: extern char _kernel_end[]

   End of entire kernel (page-aligned)

Assembly Entry Points
---------------------

.. asm:label:: _start

   Kernel entry point from bootloader.
   
   **Location:** ``boot/boot.S``
   
   **Responsibilities:**
   
   1. Disable interrupts (``csrw sie, zero``)
   2. Setup stack pointer (``la sp, _stack_top``)
   3. Clear BSS section
   4. Call ``kernel_main()``
   
   **Registers modified:**
   
   * ``sp`` - Set to ``_stack_top``
   * ``t0``, ``t1`` - Used for BSS clearing
   
   **Never returns**

See Also
--------

* :doc:`architecture` - System architecture overview
* :doc:`internals/index` - Implementation details
* :doc:`development` - Development guide
* `CHANGELOG.md <../../CHANGELOG.md>`_ - Detailed feature list
