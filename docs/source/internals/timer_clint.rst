Timer and CLINT
===============

The Core Local Interruptor (CLINT) provides timer and software interrupts for RISC-V systems. In ThunderOS, the CLINT timer is used to generate periodic timer interrupts for timekeeping and future scheduling.

Overview
--------

RISC-V systems have multiple timer sources:

* **Machine Timer (M-mode)**: Accessed directly via CLINT memory-mapped registers
* **Supervisor Timer (S-mode)**: Accessed via SBI calls to M-mode firmware
* **rdtime Instruction**: Reads current time (available in all modes)

ThunderOS runs in **Supervisor mode (S-mode)**, so it uses **SBI calls** to program the timer through OpenSBI firmware running in M-mode.

Architecture
------------

Components
~~~~~~~~~~

.. code-block:: text

   kernel/drivers/clint.c  - CLINT timer driver
   include/clint.h         - Timer interface and constants

Timer Flow
~~~~~~~~~~

.. code-block:: text

   [Kernel Init]
        |
        v
   clint_init()
        |
        ├─> Enable timer interrupts in sie register
        ├─> Enable global interrupts in sstatus
        ├─> Set first timer interrupt via SBI
        |
        v
   [Kernel Idle Loop - WFI]
        |
        v
   [Timer Fires] ──> Hardware Interrupt
        |
        v
   [Trap Handler]
        |
        v
   handle_interrupt() ──> Identifies timer interrupt (cause = 5)
        |
        v
   clint_handle_timer()
        |
        ├─> Increment tick counter
        ├─> Schedule next interrupt via SBI
        └─> Return from interrupt
        |
        v
   [Resume Kernel Execution]

SBI Timer Interface
-------------------

The Supervisor Binary Interface (SBI) provides an abstraction layer between S-mode software and M-mode firmware.

Why SBI?
~~~~~~~~

* **Privilege Separation**: S-mode kernel cannot directly access CLINT memory-mapped registers (M-mode only)
* **Portability**: Same SBI interface works across different RISC-V platforms
* **Security**: M-mode firmware mediates hardware access

SBI Call Mechanism
~~~~~~~~~~~~~~~~~~

SBI calls use the ``ecall`` instruction with arguments in registers:

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Register
     - Purpose
   * - ``a7``
     - SBI Extension ID
   * - ``a6``
     - SBI Function ID
   * - ``a0``-``a5``
     - Function arguments (a0 also returns error code)
   * - ``a1``
     - Return value

For timer:

* **Extension ID (a7)**: 0 (Legacy SET_TIMER)
* **Argument (a0)**: Absolute time value for next interrupt
* **Return (a0)**: SBI error code (0 = success)

Implementation
~~~~~~~~~~~~~~

.. code-block:: c

   // kernel/drivers/clint.c
   static int sbi_set_timer(uint64_t stime_value) {
       register unsigned long a0 asm("a0") = stime_value;
       register unsigned long a7 asm("a7") = 0;  // SBI_SET_TIMER
       
       asm volatile(
           "ecall"
           : "+r"(a0)           // Output: a0 modified (return value)
           : "r"(a7)            // Input: a7 (extension ID)
           : "memory"           // Clobber: may affect memory
       );
       
       return a0;  // Return error code
   }

**Key Points:**

* ``register unsigned long a0 asm("a0")``: Forces variable into specific register
* ``ecall``: Triggers environment call (trap to M-mode)
* ``"+r"(a0)``: a0 is both input and output
* ``"memory"`` clobber: Compiler assumes memory may change
* OpenSBI in M-mode handles the ecall and programs CLINT hardware

Reading Time
------------

The ``rdtime`` pseudo-instruction reads the current time counter.

Time Counter
~~~~~~~~~~~~

* **64-bit counter**: Increments at constant frequency
* **Frequency**: Platform-dependent (QEMU uses 10 MHz)
* **Accessible**: From all privilege levels (U/S/M-mode)
* **Read-only**: Cannot be written (M-mode can write mtime directly)

Reading Time
~~~~~~~~~~~~

.. code-block:: c

   static inline uint64_t read_time(void) {
       uint64_t time;
       asm volatile("rdtime %0" : "=r"(time));
       return time;
   }

**Note**: In RISC-V 32-bit systems, use ``rdtimeh`` to read upper 32 bits.

QEMU Timer Frequency
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Frequency: 10,000,000 Hz (10 MHz)
   Period: 100 nanoseconds per tick
   
   Examples:
   - 1 millisecond  = 10,000 ticks
   - 1 second       = 10,000,000 ticks
   - 1 minute       = 600,000,000 ticks

CLINT Driver Implementation
----------------------------

Header File
~~~~~~~~~~~

.. code-block:: c

   // include/clint.h
   #ifndef CLINT_H
   #define CLINT_H
   
   #include <stdint.h>
   
   // Timer frequency on QEMU (10 MHz)
   #define TIMER_FREQ 10000000
   
   // Timer interval in microseconds (1 second default)
   #define TIMER_INTERVAL_US 1000000
   
   void clint_init(void);
   uint64_t clint_get_ticks(void);
   void clint_set_timer(uint64_t interval_us);
   void clint_handle_timer(void);
   
   #endif

Source File
~~~~~~~~~~~

.. code-block:: c

   // kernel/drivers/clint.c
   #include "clint.h"
   #include "uart.h"
   #include <stdint.h>
   
   // Global tick counter
   static volatile uint64_t ticks = 0;
   
   // Read current time from time CSR
   static inline uint64_t read_time(void) {
       uint64_t time;
       asm volatile("rdtime %0" : "=r"(time));
       return time;
   }
   
   // Set timer via SBI call
   static int sbi_set_timer(uint64_t stime_value) {
       register unsigned long a0 asm("a0") = stime_value;
       register unsigned long a7 asm("a7") = 0;
       
       asm volatile(
           "ecall"
           : "+r"(a0)
           : "r"(a7)
           : "memory"
       );
       
       return a0;
   }
   
   void clint_init(void) {
       unsigned long sie, sstatus;
       
       // Enable timer interrupts in sie
       asm volatile("csrr %0, sie" : "=r"(sie));
       sie |= (1 << 5);  // STIE bit
       asm volatile("csrw sie, %0" :: "r"(sie));
       
       // Enable global interrupts in sstatus
       asm volatile("csrr %0, sstatus" : "=r"(sstatus));
       sstatus |= (1 << 1);  // SIE bit
       asm volatile("csrw sstatus, %0" :: "r"(sstatus));
       
       // Set first timer interrupt
       clint_set_timer(TIMER_INTERVAL_US);
       
       uart_puts("CLINT timer initialized (interval: 1 second)\n");
   }
   
   uint64_t clint_get_ticks(void) {
       return ticks;
   }
   
   void clint_set_timer(uint64_t interval_us) {
       uint64_t current = read_time();
       uint64_t ticks_interval = (TIMER_FREQ * interval_us) / 1000000;
       uint64_t next_timer = current + ticks_interval;
       
       sbi_set_timer(next_timer);
   }
   
   void clint_handle_timer(void) {
       ticks++;
       
       // Schedule next interrupt
       clint_set_timer(TIMER_INTERVAL_US);
   }

Initialization
--------------

The timer is initialized during kernel boot.

Setup Steps
~~~~~~~~~~~

1. **Enable Timer Interrupts**

   .. code-block:: c

      // Set STIE (Supervisor Timer Interrupt Enable) in sie
      unsigned long sie;
      asm volatile("csrr %0, sie" : "=r"(sie));
      sie |= (1 << 5);
      asm volatile("csrw sie, %0" :: "r"(sie));

2. **Enable Global Interrupts**

   .. code-block:: c

      // Set SIE (Supervisor Interrupt Enable) in sstatus
      unsigned long sstatus;
      asm volatile("csrr %0, sstatus" : "=r"(sstatus));
      sstatus |= (1 << 1);
      asm volatile("csrw sstatus, %0" :: "r"(sstatus));

3. **Schedule First Interrupt**

   .. code-block:: c

      // Set timer for 1 second from now
      clint_set_timer(TIMER_INTERVAL_US);

Called from ``kernel_main()``:

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       trap_init();
       clint_init();  // Initialize timer
       
       // Kernel idle loop
       while (1) {
           asm volatile("wfi");  // Wait for interrupt
       }
   }

Interrupt Handling
------------------

When a timer interrupt fires, the flow is:

1. **Hardware** jumps to ``trap_vector`` (see :doc:`trap_handler`)
2. **trap_handler()** reads ``scause`` and identifies timer interrupt (bit 63 set, cause 5)
3. **handle_interrupt()** dispatches to ``clint_handle_timer()``
4. **clint_handle_timer()** increments tick counter and schedules next interrupt
5. **trap_vector** restores context and returns via ``sret``

Handler Implementation
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void clint_handle_timer(void) {
       ticks++;
       clint_set_timer(TIMER_INTERVAL_US);
   }

This simple handler:

* Increments the global tick counter (for timekeeping)
* Schedules the next interrupt (1 second later)
* Returns quickly to minimize interrupt latency

Tick Counter
~~~~~~~~~~~~

The tick counter is:

* **Volatile**: May be modified by interrupt handler
* **64-bit**: Won't overflow for billions of years at 1 Hz
* **Atomic**: Single read/write is atomic on RISC-V 64-bit
* **Read-only from outside**: Only the interrupt handler increments it

.. code-block:: c

   static volatile uint64_t ticks = 0;
   
   uint64_t clint_get_ticks(void) {
       return ticks;
   }

Usage Examples
--------------

Reading Ticks
~~~~~~~~~~~~~

.. code-block:: c

   uint64_t start = clint_get_ticks();
   
   // Do some work...
   
   uint64_t end = clint_get_ticks();
   uint64_t elapsed = end - start;
   
   uart_puts("Elapsed seconds: ");
   print_decimal(elapsed);
   uart_puts("\n");

Reading Time
~~~~~~~~~~~~

.. code-block:: c

   uint64_t time1 = read_time();
   
   // Do some work...
   
   uint64_t time2 = read_time();
   uint64_t elapsed_ticks = time2 - time1;
   
   // Convert to microseconds (assuming 10 MHz)
   uint64_t elapsed_us = (elapsed_ticks * 1000000) / TIMER_FREQ;

Busy Wait
~~~~~~~~~

.. code-block:: c

   void delay_us(uint64_t microseconds) {
       uint64_t start = read_time();
       uint64_t ticks = (TIMER_FREQ * microseconds) / 1000000;
       
       while ((read_time() - start) < ticks) {
           // Busy wait
       }
   }

Sleep Until Interrupt
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void sleep_seconds(uint64_t seconds) {
       uint64_t target = clint_get_ticks() + seconds;
       
       while (clint_get_ticks() < target) {
           asm volatile("wfi");  // Wait for interrupt
       }
   }

CSR Registers
-------------

Timer-Related CSRs
~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - CSR
     - Name
     - Description
   * - ``time``
     - Timer
     - 64-bit real-time counter (read via ``rdtime``)
   * - ``sie``
     - Interrupt Enable
     - Bit 5 = STIE (Supervisor Timer Interrupt Enable)
   * - ``sip``
     - Interrupt Pending
     - Bit 5 = STIP (Supervisor Timer Interrupt Pending)
   * - ``sstatus``
     - Status
     - Bit 1 = SIE (Supervisor Interrupt Enable)

M-Mode CSRs (not accessible from S-mode)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - CSR
     - Description
   * - ``mtime``
     - Machine mode timer value (same as ``time``)
   * - ``mtimecmp``
     - Timer compare value (interrupt fires when mtime >= mtimecmp)

OpenSBI programs ``mtimecmp`` on behalf of S-mode via SBI calls.

Interrupt Enable Bits
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   sie register (Supervisor Interrupt Enable):
   Bit 9: SEIE (Supervisor External Interrupt Enable)
   Bit 5: STIE (Supervisor Timer Interrupt Enable)
   Bit 1: SSIE (Supervisor Software Interrupt Enable)
   
   sstatus register:
   Bit 8: SPP  (Supervisor Previous Privilege)
   Bit 5: SPIE (Supervisor Previous Interrupt Enable)
   Bit 1: SIE  (Supervisor Interrupt Enable - global)

Testing
-------

The timer has comprehensive tests in ``tests/test_timer.c``.

Test Coverage
~~~~~~~~~~~~~

.. code-block:: c

   // Verify timer interrupts are enabled in sie
   KUNIT_CASE(test_timer_interrupts_enabled)
   
   // Verify global interrupts are enabled in sstatus
   KUNIT_CASE(test_global_interrupts_enabled)
   
   // Verify initial tick count is zero
   KUNIT_CASE(test_initial_ticks_zero)
   
   // Verify rdtime instruction works
   KUNIT_CASE(test_rdtime_works)
   
   // Wait for interrupt and verify ticks increment
   KUNIT_CASE(test_timer_tick_increments)
   
   // Wait for multiple interrupts
   KUNIT_CASE(test_multiple_ticks)

Example Test
~~~~~~~~~~~~

.. code-block:: c

   static void test_timer_tick_increments(void) {
       uart_puts("Waiting for timer interrupt (1 second)...\n");
       
       uint64_t start_ticks = clint_get_ticks();
       uint64_t timeout = read_time() + 15000000;  // 1.5 second timeout
       
       // Wait for tick to increment
       while (clint_get_ticks() == start_ticks) {
           asm volatile("wfi");  // Wait for interrupt
           
           // Timeout check
           if (read_time() > timeout) {
               uart_puts("ERROR: Timer interrupt did not fire!\n");
               KUNIT_EXPECT_NE(clint_get_ticks(), start_ticks);
               return;
           }
       }
       
       KUNIT_EXPECT_EQ(clint_get_ticks(), start_ticks + 1);
   }

Running Tests
~~~~~~~~~~~~~

.. code-block:: bash

   cd tests
   make
   make run-test-timer

Expected output:

.. code-block:: text

   [ RUN      ] test_timer_interrupts_enabled
   [       OK ] test_timer_interrupts_enabled
   [ RUN      ] test_global_interrupts_enabled
   [       OK ] test_global_interrupts_enabled
   [ RUN      ] test_initial_ticks_zero
   [       OK ] test_initial_ticks_zero
   [ RUN      ] test_rdtime_works
   [       OK ] test_rdtime_works
   [ RUN      ] test_timer_tick_increments
   Waiting for timer interrupt (1 second)...
   Tick: 1
   [       OK ] test_timer_tick_increments
   [ RUN      ] test_multiple_ticks
   Waiting for 2 timer interrupts...
   Tick: 2
   Tick: 3
   [       OK ] test_multiple_ticks
   
   Total:  6
   Passed: 6
   Failed: 0

Debugging
---------

Check Timer Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void debug_timer_config(void) {
       unsigned long sie, sstatus, sip;
       
       asm volatile("csrr %0, sie" : "=r"(sie));
       asm volatile("csrr %0, sstatus" : "=r"(sstatus));
       asm volatile("csrr %0, sip" : "=r"(sip));
       
       uart_puts("sie.STIE:       "); print_bool(sie & (1 << 5));
       uart_puts("sstatus.SIE:    "); print_bool(sstatus & (1 << 1));
       uart_puts("sip.STIP:       "); print_bool(sip & (1 << 5));
   }

GDB Debugging
~~~~~~~~~~~~~

.. code-block:: gdb

   # Break on timer interrupt handler
   (gdb) break clint_handle_timer
   
   # Check CSR values
   (gdb) info registers sie
   (gdb) info registers sstatus
   (gdb) info registers scause
   
   # Read time value
   (gdb) print/x $time  # May not work in all GDB versions
   
   # Monitor ticks
   (gdb) watch ticks

Force Timer Interrupt
~~~~~~~~~~~~~~~~~~~~~

You can't directly force a timer interrupt from S-mode, but you can:

.. code-block:: c

   // Set timer to fire immediately
   uint64_t now = read_time();
   sbi_set_timer(now + 100);  // Fire in 10 microseconds

Performance
-----------

Interrupt Overhead
~~~~~~~~~~~~~~~~~~

Measured on QEMU:

* **Trap Entry**: ~50 cycles (save 34 registers + CSRs)
* **Handler**: ~20 cycles (increment, SBI call)
* **Trap Exit**: ~50 cycles (restore registers)
* **Total**: ~120 cycles per interrupt

At 1 Hz (1 interrupt/second), this is negligible overhead.

Frequency Tradeoffs
~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Frequency
     - CPU Overhead
     - Use Case
   * - 1 Hz
     - 0.001%
     - Coarse timekeeping, low-res scheduling
   * - 100 Hz
     - 0.1%
     - Traditional OS tick rate
   * - 1000 Hz
     - 1%
     - High-resolution timing, preemptive scheduling
   * - 10000 Hz
     - 10%
     - Real-time systems (probably too high for general OS)

ThunderOS currently uses **1 Hz** for simplicity.

Future Enhancements
-------------------

Planned timer improvements:

1. **Configurable Frequency**
   
   * Allow runtime adjustment of timer interval
   * Different rates for different purposes
   * Dynamic frequency scaling

2. **High-Resolution Timers**
   
   * One-shot timers for specific deadlines
   * Timer queue for multiple timeouts
   * Nanosecond-precision timing

3. **Tickless Kernel**
   
   * No periodic tick (power saving)
   * Program timer only when needed
   * On-demand scheduling decisions

4. **Time Slicing**
   
   * Preemptive multitasking
   * Round-robin scheduling
   * Process quantum enforcement

5. **Profiling Support**
   
   * Statistical profiling via timer
   * Function call timing
   * Performance counters

6. **Wall Clock Time**
   
   * Convert ticks to real time (hours, minutes, seconds)
   * RTC integration for absolute time
   * Timezone support

References
----------

* **RISC-V Privileged Specification**: Timer and interrupt details
* **SBI Specification**: Timer extension (legacy and new)
* **OpenSBI Documentation**: Timer implementation in M-mode
* **CLINT Specification**: Memory-mapped timer registers

See Also
--------

* :doc:`trap_handler` - Interrupt handling infrastructure
* :doc:`testing_framework` - How timer tests work
* :doc:`registers` - CSR register details

