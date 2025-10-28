HAL Timer Interface
===================

Overview
--------

The HAL Timer interface provides a platform-independent abstraction for timer operations, enabling the kernel to work with different timer hardware across architectures.

**Purpose**: Periodic interrupts for task scheduling, timekeeping, and delays

**Design**: Hardware-specific implementation behind portable interface

**Current Status**: ✅ Implemented for RISC-V

Interface Definition
--------------------

File: ``include/hal/hal_timer.h``

The timer HAL defines four core functions:

.. code-block:: c

   void hal_timer_init(unsigned long interval_us);
   unsigned long hal_timer_get_ticks(void);
   void hal_timer_set_next(unsigned long interval_us);
   void hal_timer_handle_interrupt(void);

API Reference
~~~~~~~~~~~~~

**hal_timer_init(interval_us)**

   Initialize timer hardware and start periodic interrupts.
   
   :param interval_us: Timer interrupt interval in microseconds
   :returns: void
   
   **Responsibilities:**
   
   * Configure hardware timer
   * Set initial interrupt interval
   * Enable timer interrupts in interrupt controller
   * Enable global interrupts if needed
   
   **Example:**
   
   .. code-block:: c
   
      // 1-second timer interrupts
      hal_timer_init(1000000);

**hal_timer_get_ticks()**

   Get the number of timer interrupts since initialization.
   
   :returns: Current tick count (unsigned long)
   
   **Use Cases:**
   
   * Measuring elapsed time
   * Scheduling decisions
   * Timeout implementation
   
   **Example:**
   
   .. code-block:: c
   
      unsigned long start = hal_timer_get_ticks();
      // ... do work ...
      unsigned long elapsed = hal_timer_get_ticks() - start;

**hal_timer_set_next(interval_us)**

   Schedule the next timer interrupt.
   
   :param interval_us: Microseconds until next interrupt
   :returns: void
   
   **Note:** Called automatically by ``hal_timer_handle_interrupt()``
   
   **Example:**
   
   .. code-block:: c
   
      // One-shot: fire in 500 milliseconds
      hal_timer_set_next(500000);

**hal_timer_handle_interrupt()**

   Handle timer interrupt (called from trap handler).
   
   :returns: void
   
   **Responsibilities:**
   
   * Increment internal tick counter
   * Perform timer bookkeeping
   * Schedule next interrupt
   
   **Note:** This is called by the architecture's interrupt handler, not by portable kernel code.

RISC-V Implementation
---------------------

File: ``kernel/arch/riscv64/drivers/timer.c``

Hardware
~~~~~~~~

**CLINT (Core Local Interruptor)**

   * Memory-mapped timer comparator
   * Accessed via SBI (Supervisor Binary Interface)
   * 64-bit cycle counter (``time`` CSR)
   * Frequency: 10 MHz on QEMU virt machine

**CSRs Used:**

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - CSR
     - Purpose
   * - ``time``
     - Read current cycle count (64-bit)
   * - ``sie``
     - Supervisor Interrupt Enable (STIE bit for timer)
   * - ``sstatus``
     - Supervisor Status (SIE bit for global interrupts)

SBI Interface
~~~~~~~~~~~~~

The implementation uses SBI ecalls to program the timer:

.. code-block:: c

   static inline void sbi_set_timer(unsigned long stime_value) {
       register unsigned long a0 asm("a0") = stime_value;
       register unsigned long a7 asm("a7") = SBI_SET_TIMER;  // 0
       asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
   }

**Why SBI?**

   In RISC-V, the timer comparator is a machine-mode (M-mode) register.
   Our kernel runs in supervisor mode (S-mode) and cannot access it directly.
   SBI provides a standardized interface for S-mode to request M-mode services.

Timer Calculation
~~~~~~~~~~~~~~~~~

Converting microseconds to timer ticks:

.. code-block:: c

   unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;

**Example:** 1-second timer on 10 MHz hardware:

.. code-block:: text

   interval_ticks = (10,000,000 * 1,000,000) / 1,000,000
                  = 10,000,000 ticks

Reading Time
~~~~~~~~~~~~

The ``rdtime`` instruction reads the 64-bit cycle counter:

.. code-block:: c

   static inline unsigned long read_time(void) {
       unsigned long time;
       asm volatile("rdtime %0" : "=r"(time));
       return time;
   }

This counter increments at a constant rate (10 MHz) regardless of CPU frequency.

Interrupt Flow
~~~~~~~~~~~~~~

1. **Initialization** (``hal_timer_init``):

   .. code-block:: text
   
      1. Calculate interval in timer ticks
      2. Read current time
      3. Set timer comparator: current_time + interval
      4. Enable STIE in sie (timer interrupt enable)
      5. Enable SIE in sstatus (global interrupts)

2. **Interrupt Occurs**:

   .. code-block:: text
   
      1. Hardware sets STIP bit in sip (timer interrupt pending)
      2. Trap handler calls hal_timer_handle_interrupt()
      3. Tick counter increments
      4. Next interrupt scheduled via sbi_set_timer()
      5. Return from trap

3. **Continuous Operation**:

   .. code-block:: text
   
      Each interrupt schedules the next, creating periodic behavior.

Code Example
~~~~~~~~~~~~

Complete initialization sequence:

.. code-block:: c

   void hal_timer_init(unsigned long interval_us) {
       timer_interval_us = interval_us;
       
       // Calculate ticks
       unsigned long interval_ticks = 
           (TIMER_FREQ * interval_us) / 1000000;
       
       // Set first interrupt
       unsigned long current_time = read_time();
       sbi_set_timer(current_time + interval_ticks);
       
       // Enable timer interrupt
       unsigned long sie;
       asm volatile("csrr %0, sie" : "=r"(sie));
       sie |= (1 << 5);  // STIE bit
       asm volatile("csrw sie, %0" :: "r"(sie));
       
       // Enable global interrupts
       unsigned long sstatus;
       asm volatile("csrr %0, sstatus" : "=r"(sstatus));
       sstatus |= (1 << 1);  // SIE bit
       asm volatile("csrw sstatus, %0" :: "r"(sstatus));
   }

Usage in Kernel
---------------

File: ``kernel/main.c``

.. code-block:: c

   void kernel_main(void) {
       hal_uart_init();
       hal_uart_puts("ThunderOS starting...\n");
       
       // Initialize trap handler
       trap_init();
       
       // Start timer: 1-second intervals
       hal_timer_init(1000000);
       
       // Idle loop - interrupts will fire
       while (1) {
           asm volatile("wfi");  // Wait for interrupt
       }
   }

**Output:**

.. code-block:: text

   ThunderOS starting...
   Timer initialized (interval: 1 second)
   Tick: 1
   Tick: 2
   Tick: 3
   ...

Trap Handler Integration
~~~~~~~~~~~~~~~~~~~~~~~~~

File: ``kernel/arch/riscv64/core/trap.c``

.. code-block:: c

   void handle_trap(struct trap_frame *tf) {
       unsigned long scause;
       asm volatile("csrr %0, scause" : "=r"(scause));
       
       if (scause & (1UL << 63)) {
           // Interrupt
           unsigned long interrupt_id = scause & 0x7FFFFFFFFFFFFFFF;
           
           if (interrupt_id == 5) {
               // Supervisor Timer Interrupt
               hal_timer_handle_interrupt();
           }
       } else {
           // Exception handling...
       }
   }

Porting Guide
-------------

To implement timer HAL for a new architecture:

1. **Identify Timer Hardware**

   * ARM64: Generic Timer (CNTPCT_EL0, CNTP_TVAL_EL0)
   * x86-64: APIC Timer, HPET, or PIT
   * RISC-V: CLINT via SBI

2. **Implement Four Functions**

   .. code-block:: c
   
      void hal_timer_init(unsigned long interval_us) {
          // Configure hardware timer
          // Enable interrupts
      }
      
      unsigned long hal_timer_get_ticks(void) {
          return global_tick_counter;
      }
      
      void hal_timer_set_next(unsigned long interval_us) {
          // Program timer comparator
      }
      
      void hal_timer_handle_interrupt(void) {
          global_tick_counter++;
          hal_timer_set_next(configured_interval);
      }

3. **Create Driver File**

   .. code-block:: text
   
      kernel/arch/<arch>/drivers/timer.c

4. **Test**

   * Verify periodic interrupts
   * Check tick count increases
   * Confirm interval accuracy

Future Enhancements
-------------------

**High-Resolution Timing**
   Nanosecond precision for performance measurements

**Delay Functions**
   ``hal_timer_delay_us()`` for busy-wait delays

**Multiple Timers**
   Support for multiple timer instances (per-CPU timers)

**Dynamic Frequency**
   Adjust timer frequency based on power management

**Tickless Kernel**
   Only schedule interrupts when needed, not periodically

See Also
--------

* :doc:`trap_handler` - Interrupt handling infrastructure
* :doc:`hal/index` - HAL overview and UART interface
* :doc:`../riscv/interrupts_exceptions` - RISC-V interrupt details
* :doc:`../riscv/privilege_levels` - SBI and privilege modes
