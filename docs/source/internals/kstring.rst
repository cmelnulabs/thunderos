Kernel String Utilities (kstring)
==================================

Overview
--------

The kernel string utilities provide helper functions for common string and number formatting operations. These utilities eliminate code duplication and provide a consistent interface for kernel code.

**Purpose:** Reduce code duplication identified by code review tools (GitHub Copilot)

**Current Functions:**

* ``kprint_dec()`` - Print decimal numbers
* ``kprint_hex()`` - Print hexadecimal numbers with 0x prefix

**Design Philosophy:**

* Simple, focused utilities
* No external dependencies (freestanding environment)

API Reference
-------------

Functions
~~~~~~~~~

**kprint_dec(n)**

   Print a decimal (base-10) number to UART.
   
   :param n: Number to print (size_t, unsigned)
   :returns: void
   
   **Behavior:**
   
   * Prints digits in decimal format
   * No leading zeros
   * No thousands separators
   * Special case: 0 prints as "0"
   
   **Examples:**
   
   .. code-block:: c
   
      kprint_dec(0);      // Output: "0"
      kprint_dec(42);     // Output: "42"
      kprint_dec(12345);  // Output: "12345"
      kprint_dec(0xFFFF); // Output: "65535"

**kprint_hex(val)**

   Print a hexadecimal (base-16) number to UART with "0x" prefix.
   
   :param val: Number to print (uintptr_t, unsigned)
   :returns: void
   
   **Behavior:**
   
   * Always prints "0x" prefix
   * Always prints 16 hex digits (64-bit width)
   * Leading zeros included (for addresses)
   * Lowercase letters (a-f)
   
   **Examples:**
   
   .. code-block:: c
   
      kprint_hex(0);          // Output: "0x0000000000000000"
      kprint_hex(42);         // Output: "0x000000000000002a"
      kprint_hex(0x80200000); // Output: "0x0000000080200000"
      kprint_hex(0xDEADBEEF); // Output: "0x00000000deadbeef"

Implementation Details
----------------------

Decimal Printing Algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

File: ``kernel/core/kstring.c``

.. code-block:: c

   void kprint_dec(size_t n) {
       if (n == 0) {
           hal_uart_putc('0');
           return;
       }
       
       // Extract digits in reverse order
       char buf[20];  // Max 20 digits for 64-bit number
       int i = 0;
       
       while (n > 0) {
           buf[i++] = '0' + (n % 10);  // Convert digit to ASCII
           n /= 10;
       }
       
       // Print in correct order (reverse of extraction)
       while (i > 0) {
           hal_uart_putc(buf[--i]);
       }
   }

**Algorithm:**

1. **Special case:** If n=0, print '0' and return
2. **Extract digits:** Repeatedly divide by 10, store remainders
3. **Convert to ASCII:** Add '0' to digit (0→'0', 1→'1', ...)
4. **Reverse print:** Digits extracted backward, print forward

**Example Trace (n=4072):**

.. code-block:: text

   Iteration 1: 4072 % 10 = 2, buf[0]='2', n=407
   Iteration 2:  407 % 10 = 7, buf[1]='7', n=40
   Iteration 3:   40 % 10 = 0, buf[2]='0', n=4
   Iteration 4:    4 % 10 = 4, buf[3]='4', n=0
   
   Print: buf[3]='4', buf[2]='0', buf[1]='7', buf[0]='2'
   Output: "4072"

**Buffer Size:**

.. code-block:: c

   // 64-bit unsigned max: 18,446,744,073,709,551,615 (20 digits)
   char buf[20];  // Sufficient for any size_t value

Hexadecimal Printing Algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void kprint_hex(uintptr_t val) {
       char hex[19];  // "0x" + 16 hex digits + null
       hex[0] = '0';
       hex[1] = 'x';
       
       // Extract each 4-bit nibble (16 nibbles for 64-bit)
       for (int i = 15; i >= 0; i--) {
           int digit = (val >> (i * 4)) & 0xF;
           hex[17 - i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
       }
       hex[18] = '\0';
       
       hal_uart_puts(hex);
   }

**Algorithm:**

1. **Prefix:** Set hex[0]='0', hex[1]='x'
2. **Extract nibbles:** Shift right by 4*i bits, mask with 0xF
3. **Convert to ASCII:**
   
   * 0-9: Add '0' (0→'0', 1→'1', ..., 9→'9')
   * 10-15: Add 'a'-10 (10→'a', 11→'b', ..., 15→'f')

4. **Print:** Output complete string via ``hal_uart_puts()``

**Example Trace (val=0x80200000):**

.. code-block:: text

   i=15: (0x80200000 >> 60) & 0xF = 0  → hex[2]='0'
   i=14: (0x80200000 >> 56) & 0xF = 0  → hex[3]='0'
   ...
   i=8:  (0x80200000 >> 32) & 0xF = 8  → hex[9]='8'
   i=7:  (0x80200000 >> 28) & 0xF = 0  → hex[10]='0'
   ...
   i=0:  (0x80200000 >> 0)  & 0xF = 0  → hex[17]='0'
   
   Result: "0x0000000080200000"

**Fixed Width:**

Always prints 16 hex digits (64-bit), useful for addresses:

.. code-block:: text

   0x0000000080200000  ← Clear alignment
   0x0000000080201000
   0x0000000087FFF000

Usage Examples
--------------

Memory Statistics
~~~~~~~~~~~~~~~~~

.. code-block:: c

   size_t total, free;
   pmm_get_stats(&total, &free);
   
   hal_uart_puts("Total pages: ");
   kprint_dec(total);
   hal_uart_puts(", Free: ");
   kprint_dec(free);
   hal_uart_puts("\n");

**Output:**

.. code-block:: text

   Total pages: 32248, Free: 32247

Address Printing
~~~~~~~~~~~~~~~~

.. code-block:: c

   uintptr_t page = pmm_alloc_page();
   hal_uart_puts("Allocated page at: 0x");
   kprint_hex(page);
   hal_uart_puts("\n");

**Output:**

.. code-block:: text

   Allocated page at: 0x0000000080200000

Combined Usage
~~~~~~~~~~~~~~

.. code-block:: c

   void *buffer = kmalloc(256);
   
   hal_uart_puts("kmalloc(");
   kprint_dec(256);
   hal_uart_puts(") returned: 0x");
   kprint_hex((uintptr_t)buffer);
   hal_uart_puts("\n");

**Output:**

.. code-block:: text

   kmalloc(256) returned: 0x0000000080200018

Future Enhancements
-------------------

Formatted Printing
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void kprintf(const char *fmt, ...);
   
   // Usage:
   kprintf("Allocated %d pages at 0x%lx\n", count, addr);

**Challenges:** Variable arguments, parsing format strings (complex for kernel)

Number Formatting Options
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void kprint_dec_padded(size_t n, int width);
   void kprint_hex_compact(uintptr_t val);  // No leading zeros
   
   kprint_dec_padded(42, 5);    // "   42"
   kprint_hex_compact(0x1234);  // "0x1234"

Binary Printing
~~~~~~~~~~~~~~~

.. code-block:: c

   void kprint_bin(uint64_t val);
   
   kprint_bin(0b10110);  // "0b00000000000000000000000000010110"

String Utilities
~~~~~~~~~~~~~~~~

.. code-block:: c

   size_t kstrlen(const char *s);
   void kstrcpy(char *dest, const char *src);
   int kstrcmp(const char *a, const char *b);
   void kmemcpy(void *dest, const void *src, size_t n);
   void kmemset(void *ptr, int value, size_t n);

**Note:** Avoid reinventing libc. Consider using compiler-builtins or minimal implementations.

Integration with Logging
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void klog(enum log_level level, const char *msg);
   
   klog(LOG_INFO, "System booting...");
   klog(LOG_WARN, "Low memory");
   klog(LOG_ERROR, "Failed to allocate page");

Design Considerations
---------------------

Why Not printf?
~~~~~~~~~~~~~~~

**Reasons for simple helpers over printf:**

1. **Implementation:** printf requires variable args, format parsing, buffering
2. **Dependencies:** Needs libc infrastructure not available in freestanding
3. **Code Size:** Minimal kernel should stay small
4. **Predictability:** Simple functions easier to debug

**Trade-off:** Less convenient, but more control and smaller footprint.

Freestanding Environment
~~~~~~~~~~~~~~~~~~~~~~~~

The kernel operates in a freestanding environment:

.. code-block:: c

   // No libc functions available:
   // ❌ printf(), sprintf(), strlen(), strcpy(), etc.
   
   // Must implement ourselves:
   // ✅ kprint_dec(), kprint_hex(), kstring functions

**Compiler Support:**

.. code-block:: bash

   gcc -ffreestanding -nostdlib -nostartfiles

This disables assumptions about hosted environment functions.

Code Organization
~~~~~~~~~~~~~~~~~

.. code-block:: text

   include/kernel/kstring.h    ← Public interface
   kernel/core/kstring.c       ← Implementation
   
   Users:
   - kernel/main.c
   - kernel/mm/pmm.c
   - kernel/mm/kmalloc.c (future)
   - kernel/arch/riscv64/drivers/timer.c (uses inline version)

Header-Only Alternative
~~~~~~~~~~~~~~~~~~~~~~~

Could use ``static inline`` in header:

.. code-block:: c

   // include/kernel/kstring.h
   static inline void kprint_dec(size_t n) {
       // ... implementation ...
   }

**Trade-offs:**

* ✅ Can be inlined and optimized by compiler
* ❌ Code duplication in each translation unit
* ❌ Larger binary if used in many files

**Decision:** Keep as regular functions for now (code size priority).

Testing
-------

Test Cases
~~~~~~~~~~

.. code-block:: c

   // test_kstring.c
   void test_kprint_dec(void) {
       kprint_dec(0);          // "0"
       kprint_dec(1);          // "1"
       kprint_dec(42);         // "42"
       kprint_dec(4072);       // "4072"
       kprint_dec(1234567890); // "1234567890"
   }
   
   void test_kprint_hex(void) {
       kprint_hex(0x0);               // "0x0000000000000000"
       kprint_hex(0x1);               // "0x0000000000000001"
       kprint_hex(0xFF);              // "0x00000000000000ff"
       kprint_hex(0x80200000);        // "0x0000000080200000"
       kprint_hex(0xDEADBEEFCAFEBABE); // "0xdeadbeefcafebabe"
   }

Manual Testing
~~~~~~~~~~~~~~

Run kernel in QEMU and verify output:

.. code-block:: bash

   make run

Expected output should match format specifications.

Known Limitations
-----------------

**No Signed Integer Support**

.. code-block:: c

   kprint_dec(-42);  // ❌ Will print large positive number
   
   // Workaround:
   if (value < 0) {
       hal_uart_putc('-');
       kprint_dec(-value);
   }

**No Padding or Width Control**

.. code-block:: c

   kprint_dec(42);  // Always prints "42", not "  42" or "042"

**Fixed Hex Width**

.. code-block:: c

   kprint_hex(0x42);  // Prints "0x0000000000000042"
                      // Not "0x42" (compact)

**No Base Control**

.. code-block:: c

   // Can't print octal (base 8) or binary (base 2)
   kprint_dec(8);  // "8", not "10" (octal) or "1000" (binary)

**Buffer Overflow Risk (Theoretical)**

If ``size_t`` or ``uintptr_t`` exceeds expected sizes, buffers could overflow. However:

* 20-byte buffer for dec: Supports up to 2^64-1 (20 digits max)
* 19-byte buffer for hex: Fixed 16 digits for 64-bit

Safe for current 64-bit RISC-V target.

See Also
--------

* :doc:`uart_driver` - Underlying UART HAL used by kstring
* :doc:`pmm` - Primary user of kstring utilities
* :doc:`kmalloc` - Uses kstring for debugging output
* :doc:`hal_timer` - Uses inline number printing (predates kstring)
