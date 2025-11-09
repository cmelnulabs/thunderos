UART Driver
===========

The UART (Universal Asynchronous Receiver/Transmitter) driver provides serial
console I/O for ThunderOS. It is the primary interface for kernel debugging
and user interaction.

Overview
--------

**Files:**
   * ``kernel/drivers/uart.c`` - Driver implementation
   * ``include/uart.h`` - Public interface

**Hardware:**
   * NS16550A compatible UART
   * Base address: 0x10000000 (QEMU virt machine)
   * No interrupts (polling mode only)

**Features:**
   * Character output (``uart_putc``)
   * String output (``uart_puts``)
   * Character input (``uart_getc``)
   * Minimal initialization

Hardware Details
----------------

NS16550A UART
~~~~~~~~~~~~~

The NS16550A is a classic UART controller, widely used and well-documented. In QEMU's virt machine, it is mapped at physical address **0x10000000**.

Register Layout
^^^^^^^^^^^^^^^

The UART has 8 registers, each **8 bits (1 byte)** wide, located consecutively in memory starting at the base address:

.. list-table::
   :header-rows: 1
   :widths: 20 20 15 45

   * - Physical Address
     - Register Name
     - Access
     - Description
   * - 0x10000000
     - RBR/THR
     - R/W
     - **Data Register**: Read to receive a byte, write to transmit a byte
   * - 0x10000001
     - IER
     - R/W
     - Interrupt Enable Register (controls which events trigger interrupts)
   * - 0x10000002
     - IIR/FCR
     - R/W
     - Interrupt ID / FIFO Control
   * - 0x10000003
     - LCR
     - R/W
     - Line Control Register (data bits, parity, stop bits)
   * - 0x10000004
     - MCR
     - R/W
     - Modem Control Register
   * - 0x10000005
     - LSR
     - R
     - **Line Status Register** (tells you if UART is ready to send/receive)
   * - 0x10000006
     - MSR
     - R
     - Modem Status Register
   * - 0x10000007
     - SCR
     - R/W
     - Scratch Register (unused, for testing)

**Memory Layout Visualization:**

.. code-block:: text

   Physical Memory:
   
   0x10000000: [ RBR/THR ]  ← Data register (1 byte)
   0x10000001: [   IER   ]  ← Interrupt enable (1 byte)
   0x10000002: [ IIR/FCR ]  ← Interrupt ID/FIFO (1 byte)
   0x10000003: [   LCR   ]  ← Line control (1 byte)
   0x10000004: [   MCR   ]  ← Modem control (1 byte)
   0x10000005: [   LSR   ]  ← Line status (1 byte) ★ Most important!
   0x10000006: [   MSR   ]  ← Modem status (1 byte)
   0x10000007: [   SCR   ]  ← Scratch (1 byte)

**How Offsets Work:**

When documentation says "offset +5", it means:

* **Base address**: 0x10000000 (where UART starts)
* **Offset +5**: Add 5 bytes to the base
* **Final address**: 0x10000000 + 5 = **0x10000005** (LSR register)

Each register is exactly **1 byte apart** in memory.

Critical Registers for ThunderOS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We only use 3 registers in the current implementation:

**1. LSR - Line Status Register (0x10000005)**

This 8-bit register tells you the UART's current state:

.. code-block:: text

   Bit:    7    6    5    4    3    2    1    0
         [  ][TX][TH][  ][  ][  ][  ][DR]
   
   Bit 0 (DR): Data Ready
      • 1 = A byte has been received and is waiting in RBR
      • 0 = No data available to read
   
   Bit 5 (THR): Transmitter Holding Register Empty
      • 1 = UART is ready to accept a new byte for transmission
      • 0 = UART is still processing the previous byte
   
   Bit 6 (TX): Transmitter Empty
      • 1 = All data has been completely transmitted
      • 0 = Transmission in progress

**2. THR - Transmitter Holding Register (0x10000000)**

* **Write-only** register (writing sends data)
* To transmit the character 'A':
  
  1. Wait until LSR bit 5 = 1 (ready to send)
  2. Write 0x41 (ASCII 'A') to address 0x10000000
  3. UART hardware sends the byte over the serial line

**3. RBR - Receiver Buffer Register (0x10000000)**

* **Read-only** register (same address as THR!)
* To receive a character:
  
  1. Wait until LSR bit 0 = 1 (data ready)
  2. Read 1 byte from address 0x10000000
  3. That byte is what was received

**Why do RBR and THR share the same address?**

* Reading from 0x10000000 → gets received data (RBR)
* Writing to 0x10000000 → sends data (THR)
* The hardware knows which register to use based on whether you read or write

Memory-Mapped I/O
~~~~~~~~~~~~~~~~~

UART registers are accessed via memory-mapped I/O:

.. code-block:: c

   #define UART0_BASE 0x10000000
   
   // Reading a register:
   volatile uint8_t *reg = (volatile uint8_t *)UART0_BASE;
   uint8_t value = *reg;
   
   // Writing a register:
   volatile uint8_t *reg = (volatile uint8_t *)UART0_BASE;
   *reg = value;

**Why volatile?**
   * Prevents compiler from optimizing away reads/writes
   * Hardware can change register values independently
   * Every access must actually hit the hardware

Source Code
-----------

uart.c
~~~~~~

.. code-block:: c

   /* kernel/drivers/uart.c */
   #include "uart.h"

   #define UART0_BASE 0x10000000
   #define UART_RBR (UART0_BASE + 0)
   #define UART_THR (UART0_BASE + 0)
   #define UART_LSR (UART0_BASE + 5)
   #define LSR_TX_IDLE (1 << 5)

   static inline void uart_write_reg(unsigned long addr, unsigned char val) {
       *(volatile unsigned char *)addr = val;
   }

   static inline unsigned char uart_read_reg(unsigned long addr) {
       return *(volatile unsigned char *)addr;
   }

   void uart_init(void) {
       // QEMU's UART is already initialized by OpenSBI
   }

   void uart_putc(char c) {
       while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
           ;
       uart_write_reg(UART_THR, c);
   }

   void uart_puts(const char *s) {
       while (*s) {
           if (*s == '\\n') {
               uart_putc('\\r');
           }
           uart_putc(*s++);
       }
   }

   char uart_getc(void) {
       while ((uart_read_reg(UART_LSR) & 1) == 0)
           ;
       return uart_read_reg(UART_RBR);
   }

uart.h
~~~~~~

.. code-block:: c

   /* include/uart.h */
   #ifndef UART_H
   #define UART_H

   void uart_init(void);
   void uart_putc(char c);
   void uart_puts(const char *s);
   char uart_getc(void);

   #endif // UART_H

Function Reference
------------------

uart_init()
~~~~~~~~~~~

.. code-block:: c

   void uart_init(void);

**Purpose:**
   Initialize the UART controller.

**Current Implementation:**
   * Does nothing (OpenSBI already configured UART)
   * Placeholder for future initialization

**Future Enhancements:**
   * Set baud rate
   * Configure data bits, parity, stop bits
   * Enable FIFOs
   * Setup interrupts

**Example:**

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       uart_puts("UART ready\\n");
   }

uart_putc()
~~~~~~~~~~~

.. code-block:: c

   void uart_putc(char c);

**Purpose:**
   Transmit a single character.

**Algorithm:**

1. Wait for transmitter to be ready (LSR bit 5 = 1)
2. Write character to THR register
3. Return

**Blocking:**
   Yes - busy-waits until UART is ready

**Example:**

.. code-block:: c

   uart_putc('H');
   uart_putc('i');
   uart_putc('\\n');

uart_puts()
~~~~~~~~~~~

.. code-block:: c

   void uart_puts(const char *s);

**Purpose:**
   Transmit a null-terminated string.

**Algorithm:**

1. For each character in string:
   
   a. If character is '\\n', send '\\r' first (CR+LF)
   b. Send character via ``uart_putc()``

2. Return when null terminator reached

**Why CR+LF?**
   * Unix uses LF ('\\n') for newline
   * Terminal emulators often need CR ('\\r') + LF for proper display
   * Converts '\\n' → '\\r\\n' automatically

**Example:**

.. code-block:: c

   uart_puts("ThunderOS booting...\\n");
   uart_puts("[OK] UART initialized\\n");

**Null Safety:**
   Does not check for NULL pointer - caller must ensure valid string

uart_getc()
~~~~~~~~~~~

.. code-block:: c

   char uart_getc(void);

**Purpose:**
   Receive a single character.

**Algorithm:**

1. Wait for data to be available (LSR bit 0 = 1)
2. Read character from RBR register
3. Return character

**Blocking:**
   Yes - waits indefinitely for input

**Example:**

.. code-block:: c

   uart_puts("Press any key: ");
   char c = uart_getc();
   uart_putc(c);
   uart_puts("\\n");

**Echo Implementation:**

.. code-block:: c

   void echo_forever(void) {
       while (1) {
           char c = uart_getc();
           uart_putc(c);
       }
   }

Implementation Details
----------------------

Polling vs. Interrupts
~~~~~~~~~~~~~~~~~~~~~~

**Current:** Polling mode

* ``uart_putc`` busy-waits for TX ready
* ``uart_getc`` busy-waits for RX data
* Simple but inefficient

**Future:** Interrupt-driven

* UART triggers interrupt when:
  
  * Byte received
  * Transmitter empty
  * Error condition

* Kernel can do other work while waiting
* Requires interrupt handler setup

Line Ending Conversion
~~~~~~~~~~~~~~~~~~~~~~~

``uart_puts`` converts Unix newlines to Windows-style:

.. code-block:: c

   if (*s == '\\n') {
       uart_putc('\\r');  // Carriage return
   }
   uart_putc(*s++);      // Line feed

**Behavior:**

.. list-table::
   :header-rows: 1

   * - Input
     - Output
     - Terminal Display
   * - ``"Hello\\n"``
     - ``"Hello\\r\\n"``
     - ``Hello`` (newline)
   * - ``"A\\nB\\n"``
     - ``"A\\r\\nB\\r\\n"``
     - | ``A``
       | ``B``

Helper Functions
~~~~~~~~~~~~~~~~

.. code-block:: c

   static inline void uart_write_reg(unsigned long addr, unsigned char val) {
       *(volatile unsigned char *)addr = val;
   }

   static inline unsigned char uart_read_reg(unsigned long addr) {
       return *(volatile unsigned char *)addr;
   }

**Purpose:**
   Encapsulate register access

**Benefits:**
   * Type safety (enforces volatile)
   * Single point of access (easier to modify)
   * Inline for zero overhead

**Why static inline?**
   * ``static`` = private to this file
   * ``inline`` = hint to compiler to inline the function
   * Combined: efficient helper functions

Usage Examples
--------------

Basic Output
~~~~~~~~~~~~

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       uart_puts("ThunderOS v0.1\\n");
       uart_puts("Kernel starting...\\n");
   }

Hexadecimal Output
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void uart_put_hex(uint64_t value) {
       const char *hex = "0123456789ABCDEF";
       uart_puts("0x");
       for (int i = 15; i >= 0; i--) {
           uart_putc(hex[(value >> (i * 4)) & 0xF]);
       }
   }

   // Usage:
   uart_put_hex(0x80200000);  // Prints: 0x0000000080200000

Simple Printf
~~~~~~~~~~~~~

.. code-block:: c

   void uart_printf(const char *fmt, ...) {
       // Future: implement basic printf
       // For now, just use uart_puts
       uart_puts(fmt);
   }

Interactive Input
~~~~~~~~~~~~~~~~~

.. code-block:: c

   void read_line(char *buffer, int max_len) {
       int i = 0;
       while (i < max_len - 1) {
           char c = uart_getc();
           if (c == '\\r' || c == '\\n') {
               break;
           }
           if (c == '\\b' && i > 0) {  // Backspace
               i--;
               uart_puts("\\b \\b");  // Erase character
               continue;
           }
           buffer[i++] = c;
           uart_putc(c);  // Echo
       }
       buffer[i] = '\\0';
       uart_puts("\\n");
   }

Limitations
-----------

Current Limitations
~~~~~~~~~~~~~~~~~~~

1. **Polling Only**
   
   * Wastes CPU while waiting
   * No concurrent I/O possible

2. **No Buffering**
   
   * Each character written immediately
   * No optimization for multiple writes

3. **No Error Handling**
   
   * Doesn't check for errors
   * No timeout on waits

4. **Fixed Configuration**
   
   * Baud rate set by firmware
   * No runtime reconfiguration

5. **Single UART**
   
   * Only UART0 supported
   * Hard-coded base address

Future Enhancements
~~~~~~~~~~~~~~~~~~~

**Interrupt-Driven I/O**

.. code-block:: c

   void uart_init(void) {
       // Enable RX and TX interrupts
       uart_write_reg(UART_IER, 0x03);
       
       // Setup interrupt handler
       register_interrupt(UART_IRQ, uart_interrupt_handler);
   }

   void uart_interrupt_handler(void) {
       uint8_t iir = uart_read_reg(UART_IIR);
       if (iir & 0x04) {  // RX data available
           char c = uart_read_reg(UART_RBR);
           rx_buffer[rx_head++] = c;
       }
       if (iir & 0x02) {  // TX ready
           if (tx_head != tx_tail) {
               uart_write_reg(UART_THR, tx_buffer[tx_tail++]);
           }
       }
   }

**Buffered I/O**

.. code-block:: c

   #define TX_BUFFER_SIZE 256
   char tx_buffer[TX_BUFFER_SIZE];
   int tx_head = 0, tx_tail = 0;

   void uart_putc(char c) {
       tx_buffer[tx_head++] = c;
       if (tx_head >= TX_BUFFER_SIZE)
           tx_head = 0;
       // Interrupt handler will drain buffer
   }

**Multiple UART Support**

.. code-block:: c

   typedef struct {
       unsigned long base;
       int irq;
       // ... buffers, state, etc.
   } uart_device_t;

   uart_device_t uart0 = { .base = 0x10000000, .irq = 10 };
   uart_device_t uart1 = { .base = 0x10000100, .irq = 11 };

   void uart_putc(uart_device_t *dev, char c);

Testing
-------

Manual Testing
~~~~~~~~~~~~~~

1. Build and run kernel
2. Verify boot messages appear
3. Test with different strings

Automated Testing
~~~~~~~~~~~~~~~~~

Use QEMU's character device to capture output:

.. code-block:: bash

   qemu-system-riscv64 ... -serial file:output.txt
   
   # Check output:
   grep "ThunderOS" output.txt

Performance Measurements
~~~~~~~~~~~~~~~~~~~~~~~~

Measure transmission time:

.. code-block:: c

   uint64_t start = read_time();
   uart_puts("Test string\\n");
   uint64_t end = read_time();
   uint64_t microseconds = (end - start) / TICKS_PER_US;

Debugging
---------

Common Issues
~~~~~~~~~~~~~

**No Output Appears**

* Check UART base address (should be 0x10000000)
* Verify OpenSBI initialized UART
* Check QEMU serial redirection

**Garbled Output**

* Baud rate mismatch
* Line ending issues (CR/LF)
* Buffer overflow in terminal

**Hangs in uart_putc**

* LSR register not accessible
* UART not initialized properly
* Infinite loop in busy-wait

GDB Debugging
~~~~~~~~~~~~~

.. code-block:: gdb

   (gdb) break uart_putc
   (gdb) continue
   (gdb) print/x *(unsigned char*)0x10000005  # Read LSR
   (gdb) x/8xb 0x10000000                     # Examine UART registers

Performance Notes
-----------------

Current Performance
~~~~~~~~~~~~~~~~~~~

At 115200 baud:

* ~11520 bytes/second
* ~87 microseconds/byte
* Busy-waiting = 100% CPU usage during I/O

Optimization Strategies
~~~~~~~~~~~~~~~~~~~~~~~

1. **Batch writes** - send multiple characters before checking status
2. **Use interrupts** - free CPU while waiting
3. **DMA transfers** - hardware copies buffer to UART
4. **Higher baud rate** - 921600 or faster (if supported)

See Also
--------

* `NS16550A Datasheet <http://www.ti.com/lit/ds/symlink/pc16550d.pdf>`_
* :doc:`bootloader` - How UART is initialized
* :doc:`../architecture` - System architecture overview
