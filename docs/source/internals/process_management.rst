Process Management
==================

This document provides a comprehensive overview of ThunderOS's process management subsystem, including process control blocks, scheduling, context switching, and the process lifecycle.

Overview
--------

ThunderOS implements preemptive multitasking with a round-robin scheduler. The process management subsystem is responsible for:

- Creating and managing process control blocks (PCBs)
- Allocating kernel and user stacks for each process
- Managing process states and transitions
- Coordinating with the scheduler for context switching
- Handling process creation, execution, and termination

The implementation is RISC-V specific and leverages supervisor mode for kernel processes, with infrastructure in place for future user-mode process support.

Architecture Components
-----------------------

The process management subsystem consists of several key components:

1. **Process Control Block (PCB)** - ``struct process`` in ``include/kernel/process.h``
2. **Process Table** - Static array of PCBs in ``kernel/core/process.c``
3. **Scheduler** - Round-robin scheduler in ``kernel/core/scheduler.c``
4. **Context Switcher** - Assembly routines in ``kernel/arch/riscv64/switch.S``
5. **Process API** - Public functions for process lifecycle management

Process Control Block (PCB)
----------------------------

Structure Definition
~~~~~~~~~~~~~~~~~~~~

The PCB (``struct process``) contains all information needed to manage a process:

.. code-block:: c

   struct process {
       pid_t pid;                          // Process ID
       proc_state_t state;                 // Process state
       char name[PROC_NAME_LEN];           // Process name (32 bytes)
       
       // Memory management
       page_table_t *page_table;           // Virtual memory page table
       uintptr_t kernel_stack;             // Kernel stack base (16KB)
       uintptr_t user_stack;               // User stack base (1MB)
       
       // Saved context (for context switching)
       struct context context;             // Kernel context
       struct trap_frame *trap_frame;      // User context (trap frame)
       
       // Scheduling
       uint64_t cpu_time;                  // Total CPU time used (in ticks)
       uint64_t priority;                  // Scheduling priority (lower = higher)
       
       // Process tree
       struct process *parent;             // Parent process
       
       // Exit status
       int exit_code;                      // Exit code if state is ZOMBIE
   };

Memory Layout
~~~~~~~~~~~~~

Each process has three separate memory regions:

1. **PCB Structure**: Stored in the static process table (392 bytes per process)
2. **Kernel Stack**: 16KB allocated via ``kmalloc()``, used for kernel-mode execution
3. **User Stack**: 1MB allocated via ``kmalloc()``, used for user-mode execution (currently unused in kernel-mode processes)
4. **Trap Frame**: Allocated separately via ``kmalloc()`` to avoid stack corruption

The kernel stack layout (grows downward):

.. code-block:: text

   High Address: kernel_stack + KERNEL_STACK_SIZE (16KB)
                 ↓
                 [Stack grows down]
                 ↓
   Low Address:  kernel_stack (base address)

Stack pointer is initialized to ``kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT`` to ensure 16-byte alignment per RISC-V ABI requirements.

Process States
--------------

Processes transition through the following states:

.. code-block:: c

   typedef enum {
       PROC_UNUSED = 0,    // Process slot is unused
       PROC_EMBRYO,        // Process being created
       PROC_READY,         // Ready to run
       PROC_RUNNING,       // Currently running
       PROC_SLEEPING,      // Waiting for event
       PROC_ZOMBIE         // Exited but not yet cleaned up
   } proc_state_t;

State Transition Diagram
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   UNUSED ──→ EMBRYO ──→ READY ⇄ RUNNING ──→ ZOMBIE ──→ UNUSED
                           ↓       ↑
                           └→ SLEEPING ──┘

State Transitions:

- **UNUSED → EMBRYO**: ``process_create()`` allocates a process slot
- **EMBRYO → READY**: Process fully initialized, added to scheduler queue
- **READY → RUNNING**: ``schedule()`` picks process for execution
- **RUNNING → READY**: Time slice expires or ``process_yield()`` called
- **RUNNING → SLEEPING**: ``process_sleep()`` called
- **SLEEPING → READY**: ``process_wakeup()`` called
- **RUNNING → ZOMBIE**: ``process_exit()`` called
- **ZOMBIE → UNUSED**: Parent reaps zombie (TODO: not yet implemented)

Context Structure
-----------------

The context structure holds callee-saved registers per RISC-V calling convention:

.. code-block:: c

   struct context {
       unsigned long ra;   // Return address
       unsigned long sp;   // Stack pointer
       unsigned long s0;   // Saved registers s0-s11
       unsigned long s1;
       unsigned long s2;
       unsigned long s3;
       unsigned long s4;
       unsigned long s5;
       unsigned long s6;
       unsigned long s7;
       unsigned long s8;
       unsigned long s9;
       unsigned long s10;
       unsigned long s11;
   };

Why Only Callee-Saved Registers?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RISC-V calling convention divides registers into:

- **Caller-saved** (a0-a7, t0-t6): Function caller must save if needed
- **Callee-saved** (s0-s11, sp, ra): Function callee must preserve

During context switching, we only need to save callee-saved registers because:

1. When a process is preempted, it's interrupted mid-function
2. The interrupted function expects callee-saved registers to be preserved
3. Caller-saved registers are already on the stack (if needed) from the function's perspective
4. The trap handler saves **all** registers (including caller-saved) in the trap frame

Process Table
-------------

The process table is a static array that holds all process control blocks:

.. code-block:: c

   #define MAX_PROCS 64
   static struct process process_table[MAX_PROCS];

Process 0 (Init Process)
~~~~~~~~~~~~~~~~~~~~~~~~~

The init process is special:

- PID 0
- Always in RUNNING state (until preempted)
- Uses the boot stack (no allocated kernel stack)
- Created during ``process_init()``
- Represents the kernel's initial execution context
- Cannot be exited

Process ID Allocation
~~~~~~~~~~~~~~~~~~~~~~

PIDs are allocated sequentially starting from 1:

.. code-block:: c

   static pid_t next_pid = 1;
   
   pid_t alloc_pid(void) {
       lock_acquire(&process_lock);
       pid_t pid = next_pid++;
       lock_release(&process_lock);
       return pid;
   }

**Note**: Current implementation has no PID wraparound or recycling. After 2³¹-1 processes, PIDs will overflow (acceptable for current design).

Process Creation
----------------

The ``process_create()`` function follows these steps:

1. **Allocate Process Slot**
   
   Find first ``PROC_UNUSED`` slot in process table:
   
   .. code-block:: c
   
      struct process *proc = alloc_process();
      if (!proc) return NULL;

2. **Assign PID and Name**
   
   .. code-block:: c
   
      proc->pid = alloc_pid();
      kstrncpy(proc->name, name, PROC_NAME_LEN - 1);

3. **Allocate Kernel Stack**
   
   16KB stack for kernel-mode execution:
   
   .. code-block:: c
   
      proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);

4. **Allocate User Stack**
   
   1MB stack for user-mode execution (currently in kernel space):
   
   .. code-block:: c
   
      proc->user_stack = (uintptr_t)kmalloc(USER_STACK_SIZE);

5. **Setup Trap Frame**
   
   Initialize trap frame with entry point and argument:
   
   .. code-block:: c
   
      proc->trap_frame = kmalloc(sizeof(struct trap_frame));
      proc->trap_frame->sepc = (unsigned long)entry_point;
      proc->trap_frame->sp = proc->user_stack + USER_STACK_SIZE;
      proc->trap_frame->a0 = (unsigned long)arg;
      proc->trap_frame->sstatus = (1 << 8) | (1 << 5);  // SPP=1, SPIE=1

6. **Initialize Context**
   
   Set up for first context switch:
   
   .. code-block:: c
   
      proc->context.ra = (unsigned long)process_wrapper;
      proc->context.sp = proc->kernel_stack + KERNEL_STACK_SIZE - STACK_ALIGNMENT;

7. **Add to Scheduler**
   
   Mark as READY and enqueue:
   
   .. code-block:: c
   
      proc->state = PROC_READY;
      scheduler_enqueue(proc);

Process Entry Wrapper
~~~~~~~~~~~~~~~~~~~~~~

The ``process_wrapper()`` function is the first kernel-mode function executed when a process runs:

.. code-block:: c

   static void process_wrapper(void) {
       struct process *proc = process_current();
       
       // Extract entry point and argument from trap frame
       void (*entry_point)(void *) = (void (*)(void *))proc->trap_frame->sepc;
       void *arg = (void *)proc->trap_frame->a0;
       
       // Call the actual process function
       entry_point(arg);
       
       // If process returns, exit gracefully
       process_exit(0);
   }

This wrapper ensures:

- Process functions are called with proper arguments
- Returning from a process function triggers clean exit
- No undefined behavior from function returns

Scheduling
----------

Round-Robin Scheduler
~~~~~~~~~~~~~~~~~~~~~~

The scheduler maintains a circular ready queue:

.. code-block:: c

   #define READY_QUEUE_SIZE MAX_PROCS
   static struct process *ready_queue[READY_QUEUE_SIZE];
   static int queue_head = 0;
   static int queue_tail = 0;
   static int queue_count = 0;

Scheduler Operations:

1. **Enqueue**: Add process to tail of ready queue
2. **Dequeue**: Remove specific process from queue (linear search)
3. **Pick Next**: Remove and return process from head of queue

Time Slicing
~~~~~~~~~~~~

Each process gets a time slice before being preempted:

.. code-block:: c

   // Time slice for round-robin scheduling
   // Units: timer ticks (1 tick = 100ms timer interval)
   // TIME_SLICE = 10 ticks = 10 * 100ms = 1000ms = 1 second per process
   #define TIME_SLICE 10
   static uint64_t current_time_slice = 0;

Timer Interrupt Flow:

1. Timer fires every 100ms
2. ``schedule()`` called from trap handler
3. Decrement ``current_time_slice``
4. If time slice reaches 0:
   - Add current process back to ready queue
   - Pick next process from queue
   - Reset time slice to ``TIME_SLICE``
   - Perform context switch

Schedule Function
~~~~~~~~~~~~~~~~~

The ``schedule()`` function implements the scheduling policy:

.. code-block:: c

   void schedule(void) {
       int old_state = interrupt_save_disable();
       
       struct process *current = process_current();
       struct process *next = NULL;
       
       // Decrement time slice
       if (current_time_slice > 0) {
           current_time_slice--;
       }
       
       // Check if we should preempt current process
       int should_preempt = 0;
       
       if (!current) {
           should_preempt = 1;  // No current process
       } else if (current->state != PROC_RUNNING) {
           should_preempt = 1;  // Process sleeping/zombie
       } else if (current_time_slice == 0) {
           should_preempt = 1;  // Time slice expired
           current_time_slice = TIME_SLICE;
       }
       
       if (should_preempt) {
           next = scheduler_pick_next();
           
           if (!next) {
               // No process to run, idle
               interrupt_restore(old_state);
               return;
           }
           
           // Add current back to ready queue if still running
           if (current && current->state == PROC_RUNNING && current != next) {
               scheduler_enqueue(current);
           }
           
           // Switch to next process
           if (next != current) {
               context_switch(current, next);
           }
       }
       
       interrupt_restore(old_state);
   }

Context Switching
-----------------

Context switching is performed in two layers:

1. **High-level**: ``context_switch()`` in C
2. **Low-level**: ``context_switch_asm()`` in assembly

Context Switch Flow
~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Timer Interrupt → trap_handler() → schedule() → context_switch() → context_switch_asm()
                                                                              ↓
                                                                    Save old context
                                                                    Load new context
                                                                         ret
                                                                              ↓
                                                    ← ← ← ← ← ← ← ← ← ← ← ← ← ←
                                                    (returns to new process)

High-Level Context Switch
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``context_switch()`` function handles state management:

.. code-block:: c

   void context_switch(struct process *old, struct process *new) {
       if (!new) return;
       
       // Update states (interrupts must be disabled by caller)
       if (old && old->state == PROC_RUNNING) {
           old->state = PROC_READY;
       }
       new->state = PROC_RUNNING;
       
       // Set current process
       process_set_current(new);
       
       // TODO: Switch page tables if different
       // if (old && old->page_table != new->page_table) {
       //     switch_page_table(new->page_table);
       // }
       
       // Perform low-level context switch
       if (old) {
           context_switch_asm(&old->context, &new->context);
       } else {
           context_switch_asm(NULL, &new->context);
       }
   }

**Critical Requirement**: This function MUST be called with interrupts disabled to ensure atomic state updates and prevent race conditions.

Low-Level Context Switch (Assembly)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``context_switch_asm()`` function in ``switch.S`` handles register saving/restoring:

.. code-block:: asm

   context_switch_asm:
       # ========================================
       # Save old context (if old != NULL)
       # ========================================
       beqz a0, load_new
       
       # Save callee-saved registers to old context
       sd ra, 0(a0)      # Return address
       sd sp, 8(a0)      # Stack pointer
       sd s0, 16(a0)     # Saved register s0
       sd s1, 24(a0)     # Saved register s1
       sd s2, 32(a0)     # Saved register s2
       sd s3, 40(a0)     # Saved register s3
       sd s4, 48(a0)     # Saved register s4
       sd s5, 56(a0)     # Saved register s5
       sd s6, 64(a0)     # Saved register s6
       sd s7, 72(a0)     # Saved register s7
       sd s8, 80(a0)     # Saved register s8
       sd s9, 88(a0)     # Saved register s9
       sd s10, 96(a0)    # Saved register s10
       sd s11, 104(a0)   # Saved register s11
       
   load_new:
       # ========================================
       # Load new context from a1
       # ========================================
       ld ra, 0(a1)      # Return address
       ld sp, 8(a1)      # Stack pointer
       ld s0, 16(a1)     # Saved register s0
       ld s1, 24(a1)     # Saved register s1
       ld s2, 32(a1)     # Saved register s2
       ld s3, 40(a1)     # Saved register s3
       ld s4, 48(a1)     # Saved register s4
       ld s5, 56(a1)     # Saved register s5
       ld s6, 64(a1)     # Saved register s6
       ld s7, 72(a1)     # Saved register s7
       ld s8, 80(a1)     # Saved register s8
       ld s9, 88(a1)     # Saved register s9
       ld s10, 96(a1)    # Saved register s10
       ld s11, 104(a1)   # Saved register s11
       
       # ========================================
       # Return to new context
       # ========================================
       ret

Register Layout in Context Structure:

.. code-block:: text

   Offset  Register  Size
   ------  --------  ----
   0       ra        8 bytes
   8       sp        8 bytes
   16      s0        8 bytes
   24      s1        8 bytes
   32      s2        8 bytes
   40      s3        8 bytes
   48      s4        8 bytes
   56      s5        8 bytes
   64      s6        8 bytes
   72      s7        8 bytes
   80      s8        8 bytes
   88      s9        8 bytes
   96      s10       8 bytes
   104     s11       8 bytes
   
   Total: 112 bytes

First Context Switch (New Process)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When a newly created process runs for the first time:

1. **Old process context saved** (if not NULL)
2. **New process context loaded**:
   
   - ``ra`` = ``process_wrapper`` (return address)
   - ``sp`` = top of kernel stack (aligned)
   - ``s0-s11`` = 0 (initialized to zero)

3. **ret instruction executes**: Jumps to address in ``ra`` (``process_wrapper``)
4. **process_wrapper executes**: Calls the actual process function

Subsequent Context Switches
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For running processes being preempted:

1. **Timer interrupt fires**: Saves all registers to trap frame
2. **Trap handler calls schedule()**: Which calls ``context_switch()``
3. **Current context saved**: ra, sp, s0-s11 saved to current process
4. **Next context loaded**: ra, sp, s0-s11 loaded from next process
5. **ret executes**: Returns to wherever next process was interrupted
6. **Trap handler returns**: Restores all registers from trap frame

Process Lifecycle Management
-----------------------------

Process Yield
~~~~~~~~~~~~~

Voluntarily give up CPU:

.. code-block:: c

   void process_yield(void) {
       extern void schedule(void);
       schedule();
   }

This is cooperative multitasking - the process voluntarily calls ``schedule()`` which may switch to another process.

Process Exit
~~~~~~~~~~~~

Terminate the current process:

.. code-block:: c

   void process_exit(int exit_code) {
       struct process *proc = current_process;
       
       if (!proc || proc->pid == 0) {
           // Can't exit init process
           while (1) __asm__ volatile("wfi");
       }
       
       lock_acquire(&process_lock);
       proc->state = PROC_ZOMBIE;
       proc->exit_code = exit_code;
       lock_release(&process_lock);
       
       // Schedule another process (never returns)
       process_yield();
       
       // Should never reach here
       while (1) __asm__ volatile("wfi");
   }

**TODO**: Zombie process reaping not yet implemented. Processes remain in ZOMBIE state indefinitely.

Process Sleep/Wakeup
~~~~~~~~~~~~~~~~~~~~~

Sleep for a number of timer ticks:

.. code-block:: c

   void process_sleep(uint64_t ticks) {
       struct process *proc = current_process;
       if (!proc) return;
       
       lock_acquire(&process_lock);
       proc->state = PROC_SLEEPING;
       // TODO: Add to sleep queue with wakeup time
       lock_release(&process_lock);
       
       process_yield();
   }

Wake up a sleeping process:

.. code-block:: c

   void process_wakeup(struct process *proc) {
       if (!proc) return;
       
       lock_acquire(&process_lock);
       if (proc->state == PROC_SLEEPING) {
           proc->state = PROC_READY;
           scheduler_enqueue(proc);
       }
       lock_release(&process_lock);
   }

**TODO**: Timer-based sleep queue not implemented. Sleeping processes must be manually woken up.

Synchronization
---------------

Process Lock
~~~~~~~~~~~~

A simple spinlock protects the process table:

.. code-block:: c

   static volatile int process_lock = 0;
   
   static inline void lock_acquire(volatile int *lock) {
       while (__sync_lock_test_and_set(lock, 1)) {
           // Spin
       }
   }
   
   static inline void lock_release(volatile int *lock) {
       __sync_lock_release(lock);
   }

Uses GCC atomic builtin functions for atomic test-and-set operations.

Scheduler Lock
~~~~~~~~~~~~~~

Separate spinlock protects the ready queue:

.. code-block:: c

   static volatile int sched_lock = 0;

Both locks are simple spinlocks - no priority inversion handling or deadlock prevention.

Interrupt Disabling
~~~~~~~~~~~~~~~~~~~

Critical sections in ``schedule()`` and ``context_switch()`` disable interrupts:

.. code-block:: c

   int old_state = interrupt_save_disable();
   // ... critical section ...
   interrupt_restore(old_state);

This prevents:

- Timer interrupts during scheduling decisions
- Race conditions in state updates
- Inconsistent context switches

Memory Management Integration
------------------------------

Page Tables
~~~~~~~~~~~

Currently, all processes share the kernel page table:

.. code-block:: c

   proc->page_table = get_kernel_page_table();

**TODO**: User processes need separate page tables for memory isolation.

Stack Allocation
~~~~~~~~~~~~~~~~

Stacks are allocated via ``kmalloc()``:

- **Kernel stack**: 16KB via ``kmalloc(KERNEL_STACK_SIZE)``
- **User stack**: 1MB via ``kmalloc(USER_STACK_SIZE)``
- **Trap frame**: Allocated separately to prevent stack corruption

Stack allocation uses multi-page allocation from PMM:

.. code-block:: c

   // In kmalloc.c
   if (size > PAGE_SIZE) {
       size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
       uintptr_t pages = pmm_alloc_pages(num_pages);
       // ...
   }

Trap Frame Allocation
~~~~~~~~~~~~~~~~~~~~~

The trap frame is allocated separately (not on stack) to prevent corruption:

.. code-block:: c

   proc->trap_frame = (struct trap_frame *)kmalloc(sizeof(struct trap_frame));

This was changed from stack-based allocation after discovering stack growth could corrupt the trap frame during process execution.

Performance Considerations
--------------------------

Time Complexity
~~~~~~~~~~~~~~~

- **Process creation**: O(n) - linear search for free slot
- **Process lookup**: O(n) - linear search by PID
- **Scheduler enqueue**: O(1) - append to tail
- **Scheduler dequeue**: O(n) - linear search to remove
- **Scheduler pick next**: O(1) - remove from head
- **Context switch**: O(1) - fixed number of register saves/loads

Memory Overhead
~~~~~~~~~~~~~~~

Per process:

- PCB structure: 392 bytes (in static array)
- Kernel stack: 16 KB
- User stack: 1 MB
- Trap frame: ~264 bytes
- **Total**: ~1.04 MB per process

Maximum 64 processes = ~66 MB maximum overhead

Optimization Opportunities
~~~~~~~~~~~~~~~~~~~~~~~~~~

1. **Use hash table for PID lookup**: O(1) instead of O(n)
2. **Priority queue for scheduler**: Better scheduling decisions
3. **Separate user/kernel page tables**: Memory isolation
4. **Stack guard pages**: Detect stack overflows
5. **Timer-based sleep queue**: Efficient sleeping processes
6. **Process recycling**: Reuse ZOMBIE process slots

Debugging Support
-----------------

Process Table Dump
~~~~~~~~~~~~~~~~~~~

The ``process_dump()`` function prints all active processes:

.. code-block:: c

   void process_dump(void) {
       hal_uart_puts("\n=== Process Table ===\n");
       hal_uart_puts("PID  State     Name\n");
       hal_uart_puts("---  --------  --------\n");
       
       for (int i = 0; i < MAX_PROCS; i++) {
           struct process *p = &process_table[i];
           if (p->state != PROC_UNUSED) {
               // Print PID, state, name
           }
       }
   }

Example output:

.. code-block:: text

   === Process Table ===
   PID  State     Name
   ---  --------  --------
   0    RUNNING   init
   1    READY     proc_a
   2    READY     proc_b
   3    READY     proc_c

Current Process Tracking
~~~~~~~~~~~~~~~~~~~~~~~~~

Global variable tracks currently running process:

.. code-block:: c

   static struct process *current_process = NULL;
   
   struct process *process_current(void) {
       return current_process;
   }

This is updated by ``process_set_current()`` during context switches.

Limitations and Future Work
----------------------------

Current Limitations
~~~~~~~~~~~~~~~~~~~

1. **No user-mode processes**: All processes run in supervisor mode
2. **No memory isolation**: All processes share kernel page table
3. **No process priorities**: Pure round-robin scheduling
4. **No IPC mechanisms**: Processes cannot communicate
5. **No signal handling**: Cannot send signals to processes
6. **No zombie reaping**: Exited processes stay in memory
7. **No fork/exec**: Cannot create child processes or load programs
8. **Fixed time slice**: Cannot adjust per-process scheduling quantum
9. **No CPU affinity**: Single CPU only
10. **No real-time support**: No deadline or priority scheduling

Future Enhancements
~~~~~~~~~~~~~~~~~~~

1. **User Mode Processes**
   
   - Implement separate user/kernel page tables
   - Add privilege level switching in trap handler
   - Support user-mode system calls

2. **Fork and Exec**
   
   - Implement ``process_fork()`` for process cloning
   - Implement ``process_exec()`` for program loading
   - Add ELF loader for executable files

3. **Inter-Process Communication**
   
   - Implement pipes for process communication
   - Add shared memory support
   - Implement message queues

4. **Advanced Scheduling**
   
   - Priority-based scheduling
   - Multi-level feedback queue
   - Real-time scheduling policies (FIFO, RR, Deadline)

5. **Process Management**
   
   - Wait/waitpid for zombie reaping
   - Signal handling (SIGKILL, SIGTERM, etc.)
   - Process groups and sessions

6. **Resource Limits**
   
   - CPU time limits
   - Memory limits
   - File descriptor limits

7. **Multi-core Support**
   
   - SMP scheduling
   - CPU affinity
   - Load balancing

Example Usage
-------------

Creating a Process
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void my_process(void *arg) {
       int count = 0;
       while (1) {
           hal_uart_puts("Process running\n");
           kprint_dec(count++);
           process_yield();
       }
   }
   
   void kernel_main(void) {
       // Initialize process management
       process_init();
       scheduler_init();
       
       // Create a process
       struct process *proc = process_create("my_proc", my_process, NULL);
       if (proc) {
           hal_uart_puts("Process created with PID ");
           kprint_dec(proc->pid);
           hal_uart_puts("\n");
       }
       
       // Enable timer interrupts for preemption
       hal_timer_init(TIMER_INTERVAL_US);
       
       // Idle loop
       while (1) {
           __asm__ volatile("wfi");
       }
   }

Process Lifecycle Example
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void worker_process(void *arg) {
       int task_id = (int)(uintptr_t)arg;
       
       hal_uart_puts("Worker ");
       kprint_dec(task_id);
       hal_uart_puts(" started\n");
       
       for (int i = 0; i < 10; i++) {
           hal_uart_puts("Worker ");
           kprint_dec(task_id);
           hal_uart_puts(" iteration ");
           kprint_dec(i);
           hal_uart_puts("\n");
           
           process_yield();
       }
       
       hal_uart_puts("Worker ");
       kprint_dec(task_id);
       hal_uart_puts(" exiting\n");
       
       process_exit(0);
   }
   
   void create_workers(void) {
       for (int i = 0; i < 3; i++) {
           char name[32];
           kstrcpy(name, "worker_");
           name[7] = '0' + i;
           name[8] = '\0';
           
           process_create(name, worker_process, (void *)(uintptr_t)i);
       }
   }

References
----------

- ``include/kernel/process.h`` - Process management API
- ``include/kernel/scheduler.h`` - Scheduler API
- ``kernel/core/process.c`` - Process management implementation
- ``kernel/core/scheduler.c`` - Scheduler implementation
- ``kernel/arch/riscv64/switch.S`` - Context switching assembly
- RISC-V Calling Convention: https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
- RISC-V Privileged Specification: https://riscv.org/technical/specifications/
