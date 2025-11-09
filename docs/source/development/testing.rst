Testing Guide
=============

This document describes the comprehensive testing framework and how to run tests for ThunderOS.

Quick Start
-----------

Run the automated test suite:

.. code-block:: bash

   cd /path/to/thunderos
   tests/test_syscalls.sh

Expected output:

.. code-block:: text

   [*] Building kernel...
   [✓] Build successful
   [✓] Kernel ELF found
   [*] Running QEMU test...
   ...
   [✓] All critical tests passed!

Test Framework Overview
-----------------------

ThunderOS includes multiple automated test scripts in the ``tests/`` directory:

**Integration Tests:**

- ``test_qemu.sh`` - Basic QEMU boot and kernel functionality
- ``test_syscalls.sh`` - Comprehensive syscall testing (6 automated checks)
- ``test_user_mode.sh`` - User-mode process execution and privilege separation
- ``test_user_quick.sh`` - Fast user-mode validation (4 checks)

**Unit Tests:**

- ``test_trap.c`` - Trap handler unit tests
- ``test_timer.c`` - Timer interrupt unit tests
- ``test_paging.c`` - Memory paging unit tests

**Features:**

- Clean compilation (``make clean && make``)
- QEMU execution with configurable timeouts
- Output capture to ``tests/*.txt`` files
- Automated validation checks with color-coded results
- CI/CD integration via GitHub Actions

Test Output Analysis
--------------------

Test results are saved to ``tests/*.txt`` files:

.. code-block:: bash

   # View test outputs
   ls tests/*.txt
   
   # View last 50 lines of syscall test
   tail -50 tests/thunderos_test_output.txt

   # Search for specific strings
   grep "Process A" tests/thunderos_test_output.txt
   grep "User process" tests/thunderos_test_output.txt

Expected Process Output
~~~~~~~~~~~~~~~~~~~~~~~

**Kernel Boot Phase:**

.. code-block:: text

   OpenSBI v0.9
   ...
   [OK] UART initialized
   [OK] Interrupt subsystem initialized
   Trap handler initialized
   [OK] Timer interrupts enabled

**Process Creation:**

.. code-block:: text

   [OK] Created Process A (PID 1)
   [OK] Created Process B (PID 2)
   [OK] Created Process C (PID 3)
   [OK] Created user process (PID 4)

**Multitasking:**

.. code-block:: text

   [Process A] Running... iteration 0
   [Process B] Hello from B! count = 0
   [Process C] Task C executing... #0
   [Process A] Running... iteration 1
   [Process B] Hello from B! count = 1
   ...

Manual Testing
--------------

Running QEMU Directly
~~~~~~~~~~~~~~~~~~~~~

For interactive testing:

.. code-block:: bash

   qemu-system-riscv64 \
       -machine virt \
       -m 128M \
       -nographic \
       -serial mon:stdio \
       -bios default \
       -kernel build/thunderos.elf

Press ``Ctrl+A`` followed by ``X`` to exit QEMU.

Debugging with GDB
~~~~~~~~~~~~~~~~~~

1. Terminal 1 - Start QEMU in debug mode:

   .. code-block:: bash

      make debug

2. Terminal 2 - Connect with GDB:

   .. code-block:: bash

      riscv64-unknown-elf-gdb build/thunderos.elf
      (gdb) target remote :1234
      (gdb) break handle_exception
      (gdb) continue

Useful GDB Commands:

.. code-block:: text

   (gdb) info registers        # Show register values
   (gdb) x/10i $pc             # Disassemble at PC
   (gdb) stepi                 # Single-step instruction
   (gdb) backtrace             # Show call stack
   (gdb) break process_yield   # Set breakpoint

User-Space Testing
-------------------

Testing sys_write()
~~~~~~~~~~~~~~~~~~~

The ``tests/user_hello.c`` program demonstrates user-space syscalls:

.. code-block:: c

   void user_main(void) {
       print_string("=================================\n");
       print_string("ThunderOS User Space Hello World\n");
       print_string("=================================\n\n");
       
       print_string("Process Information:\n");
       print_string("  Current PID:  ");
       print_int(getpid());
       print_string("\n");
       
       exit(0);
   }

**Testing Output:**

The program runs as PID 4 and produces console output through ``sys_write()``.

Look for output like:

.. code-block:: text

   =================================
   ThunderOS User Space Hello World
   =================================
   
   Process Information:
     Current PID:  4
     Parent PID:   0
     System time:  0s

Testing sys_getpid()
~~~~~~~~~~~~~~~~~~~~

Verify process IDs are correct:

.. code-block:: bash

   # Run test and search for PID output
   ./test_syscalls.sh 2>&1 | grep "PID"

Expected PIDs:
- Kernel processes: 0 (init), 1-3 (demo processes)
- User process: 4

Testing sys_exit()
~~~~~~~~~~~~~~~~~~

Verify user programs exit cleanly:

.. code-block:: bash

   # Run test and check for exit messages
   tail -100 /tmp/thunderos_test_output.txt | grep -i "exit\|terminating"

The user program (PID 4) should exit without crashing the kernel.

Syscall Validation Checklist
-----------------------------

After running tests, verify:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Syscall
     - Test Status
     - Expected Behavior

   * - SYS_EXIT (0)
     - ✓ PASS
     - Process terminates cleanly

   * - SYS_WRITE (1)
     - ✓ PASS
     - Output appears on UART/console

   * - SYS_READ (2)
     - ⚠ Partial
     - Returns 0 (EOF) - input not implemented

   * - SYS_GETPID (3)
     - ✓ PASS
     - Returns correct process ID

   * - SYS_SLEEP (5)
     - ⚠ Partial
     - Yields to scheduler (timer not implemented)

   * - SYS_YIELD (6)
     - ✓ PASS
     - Voluntarily yields CPU

   * - SYS_GETPPID (10)
     - ✓ PASS
     - Returns parent PID

   * - SYS_GETTIME (12)
     - ✓ PASS
     - Returns system time in milliseconds

Troubleshooting
---------------

Test Fails to Compile
~~~~~~~~~~~~~~~~~~~~~

**Error:** ``make: command not found``

**Solution:** Ensure you're in the ThunderOS directory and have the build environment setup.

.. code-block:: bash

   cd /home/christian/Proyectos/thunderos
   make clean
   make

**Error:** Cross-compiler not found

**Solution:** Install or add RISC-V toolchain to PATH:

.. code-block:: bash

   # Check toolchain
   which riscv64-unknown-elf-gcc
   
   # If not found, install or add to PATH
   export PATH=$PATH:/opt/riscv/bin

QEMU Doesn't Start
~~~~~~~~~~~~~~~~~~~

**Error:** ``qemu-system-riscv64: command not found``

**Solution:** Install QEMU with RISC-V support:

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install qemu-system-riscv64
   
   # macOS
   brew install qemu

Test Hangs
~~~~~~~~~~

**Issue:** Test doesn't exit after timeout

**Solution:** The timeout should trigger automatically. If not:

1. Press ``Ctrl+C`` in terminal
2. Check QEMU process:

   .. code-block:: bash

      ps aux | grep qemu
      kill <PID>

3. Check output file:

   .. code-block:: bash

      tail tests/thunderos_test_output.txt

QEMU Crashes
~~~~~~~~~~~~

**Error:** ``Segmentation fault in QEMU``

**Possible causes:**
- Invalid kernel binary
- Memory corruption in kernel
- Infinite loop in trap handler

**Debugging steps:**

1. Check build output for warnings
2. Run with GDB for exact location
3. Review recent changes to trap.c or process.c

Performance Testing
-------------------

Measuring Syscall Latency
~~~~~~~~~~~~~~~~~~~~~~~~~

Add timing measurements in user programs:

.. code-block:: c

   uint64_t start = gettime();
   write(STDOUT_FD, "test", 4);
   uint64_t end = gettime();
   
   print_string("Syscall latency: ");
   print_int(end - start);
   print_string("ms\n");

Expected latency: < 1ms on QEMU

Measuring Context Switch Overhead
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use the scheduler statistics:

.. code-block:: bash

   # Build and run, then measure process interleaving frequency
   ./test_syscalls.sh 2>&1 | grep "Process" | wc -l

Higher interleaving = faster context switches

Continuous Integration
----------------------

For CI/CD pipelines:

.. code-block:: bash

   #!/bin/bash
   set -e
   
   cd /home/christian/Proyectos/thunderos
   tests/test_syscalls.sh
   
   # Check for expected keywords
   grep -q "All critical tests passed" tests/thunderos_test_output.txt
   
   echo "CI test passed!"

The GitHub Actions workflow (``.github/workflows/ci.yml``) runs all test scripts automatically on:

- Push to main, dev/*, feature/*, release/* branches
- Pull requests to main and dev/* branches

Test artifacts are uploaded for debugging failed builds.
