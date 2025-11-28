Testing Guide
=============

This document describes the comprehensive testing framework and how to run tests for ThunderOS.

Quick Start
-----------

Run the automated test suite:

.. code-block:: bash

   cd /path/to/thunderos
   make test

Or run individual test scripts:

.. code-block:: bash

   # All tests
   tests/scripts/run_all_tests.sh
   
   # Individual test suites
   tests/scripts/test_boot.sh          # Quick boot validation
   tests/scripts/test_kernel.sh        # Comprehensive kernel test
   tests/scripts/test_integration.sh   # Full integration tests

Expected output:

.. code-block:: text

   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     Running ThunderOS Test Suite
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   
   [*] Running boot tests...
   ✓ Boot test passed
   
   [*] Running integration tests...
   ✓ Integration test passed
   
   [*] Running user mode tests...
   ✓ User mode test passed
   
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     Test Suite Complete: 3/3 passed
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Test Framework Overview
-----------------------

ThunderOS includes comprehensive automated testing in the ``tests/`` directory:

**Test Structure:**

.. code-block:: text

   tests/
   ├── framework/          # KUnit-inspired test framework
   │   ├── kunit.h        # Test macros and assertions
   │   └── kunit.c        # Test runner implementation
   ├── unit/              # Unit tests (built into kernel)
   │   ├── test_memory_mgmt.c      # Memory management (PMM, kmalloc, DMA)
   │   ├── test_memory_isolation.c # Process isolation (15 tests)
   │   └── test_elf.c              # ELF loader validation
   ├── scripts/           # Automated test scripts
   │   ├── run_all_tests.sh       # Master test runner
   │   ├── test_boot.sh           # Quick boot validation (6 checks)
   │   ├── test_kernel.sh         # Comprehensive kernel test (17 checks)
   │   └── test_integration.sh    # Full system integration
   └── outputs/           # Test result logs

**Unit Tests (Kernel-Embedded):**

ThunderOS uses kernel-embedded unit tests that run at boot when ``ENABLE_KERNEL_TESTS=1`` (default):

1. **test_memory_mgmt.c** (10 tests):
   - Physical memory manager (PMM) allocation/deallocation
   - Kernel heap allocator (kmalloc/kfree)
   - DMA allocator for device I/O
   - Page alignment and zeroing

2. **test_memory_isolation.c** (15 tests):
   - Per-process page tables
   - Virtual Memory Areas (VMAs)
   - Process heap isolation (sys_brk)
   - Memory protection enforcement
   - Fork memory copying

3. **test_elf.c** (8 tests):
   - ELF header validation
   - Architecture verification (EM_RISCV)
   - Program header parsing
   - Segment loading
   - Entry point validation

**Integration Tests (Shell Scripts):**

Automated QEMU-based tests verify end-to-end functionality:

- ``test_boot.sh`` - Quick boot validation (6 checks)
- ``test_kernel.sh`` - Comprehensive kernel test (17 checks)
- ``test_integration.sh`` - VirtIO, ext2, shell, file operations

**Features:**

- Clean compilation (``make clean && make``)
- QEMU execution with automatic timeout detection
- Output capture to ``tests/outputs/*.txt`` files
- Automated validation checks with color-coded results
- CI/CD integration via GitHub Actions (``make test``)

Test Output Analysis
--------------------

Test results are saved to ``tests/outputs/`` directory:

.. code-block:: bash

   # View test outputs
   ls tests/outputs/
   
   # View boot test results
   cat tests/outputs/boot_test_output.txt
   
   # View integration test results
   cat tests/outputs/integration_test_output.txt
   
   # Search for specific patterns
   grep "PASS\|FAIL" tests/outputs/boot_test_output.txt
   grep "ext2" tests/outputs/integration_test_output.txt

Expected Boot Test Output
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Unit Tests at Boot:**

.. code-block:: text

   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     Running ThunderOS Unit Tests
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   
   [Memory Management Tests]
   ✓ PASS: PMM allocation
   ✓ PASS: PMM deallocation
   ✓ PASS: kmalloc basic
   ✓ PASS: kmalloc large
   ✓ PASS: DMA allocation
   ✓ PASS: DMA alignment
   ✓ PASS: DMA zeroing
   
   [Memory Isolation Tests]
   ✓ PASS: Isolated page table
   ✓ PASS: VMA tracking
   ✓ PASS: Heap isolation (brk)
   ✓ PASS: Fork memory copy
   ✓ PASS: Process cleanup
   
   [ELF Loader Tests]
   ✓ PASS: ELF magic validation
   ✓ PASS: Architecture check (EM_RISCV)
   ✓ PASS: Executable type (ET_EXEC)
   
   Tests: 33 passed, 0 failed

**Kernel Initialization:**

.. code-block:: text

   [OK] UART initialized
   [OK] Physical Memory Manager initialized
   [OK] Kernel heap initialized
   [OK] Paging enabled (Sv39)
   [OK] Interrupt subsystem initialized
   [OK] Timer interrupts enabled
   [OK] VirtIO block device initialized
   [OK] ext2 filesystem mounted at /

Manual Testing
--------------

Running QEMU Directly
~~~~~~~~~~~~~~~~~~~~~

For manual testing with VirtIO and ext2 filesystem (requires QEMU 10.1.2+):

.. code-block:: bash

   # Build kernel and filesystem
   make clean && make
   make fs
   
   # Run with VirtIO block device
   qemu-system-riscv64 \
     -machine virt \
     -m 128M \
     -nographic \
     -serial mon:stdio \
     -bios none \
     -kernel build/thunderos.elf \
     -global virtio-mmio.force-legacy=false \
     -drive file=build/fs.img,if=none,format=raw,id=hd0 \
     -device virtio-blk-device,drive=hd0

.. warning::
   **CRITICAL:** Always include ``-global virtio-mmio.force-legacy=false``
   
   Without this flag, VirtIO I/O will timeout and the filesystem won't mount!

Press ``Ctrl+A`` followed by ``X`` to exit QEMU.

**Interactive Shell:**

Once booted, you can use the interactive shell:

.. code-block:: text

   thunderos> ls
   test.txt
   README.txt
   bin/
   
   thunderos> cat test.txt
   Hello from ThunderOS ext2 filesystem!
   
   thunderos> cat /bin/hello
   (executes hello program from filesystem)

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

Testing User Programs
~~~~~~~~~~~~~~~~~~~~~

ThunderOS can execute ELF binaries from the ext2 filesystem. User programs are in ``userland/``:

.. code-block:: text

   userland/
   ├── cat.c           # File concatenation utility
   ├── ls.c            # Directory listing
   ├── hello.c         # Hello world demo
   ├── signal_test.c   # Signal handling test
   └── build/          # Compiled binaries

**Building Userland:**

.. code-block:: bash

   ./build_userland.sh

**Running from Shell:**

.. code-block:: text

   thunderos> cat /test.txt
   Hello from ThunderOS ext2 filesystem!
   
   thunderos> /bin/hello
   (program executes and prints output)

Testing System Calls
~~~~~~~~~~~~~~~~~~~~~

User programs test the syscall interface:

**Available Syscalls (13 implemented):**

.. code-block:: c

   SYS_EXIT    = 0   // Exit process
   SYS_WRITE   = 1   // Write to file descriptor
   SYS_READ    = 2   // Read from file descriptor
   SYS_OPEN    = 13  // Open file
   SYS_CLOSE   = 14  // Close file descriptor
   SYS_GETPID  = 3   // Get process ID
   SYS_FORK    = 4   // Fork process
   SYS_EXECVE  = 11  // Execute program
   SYS_WAITPID = 7   // Wait for child
   SYS_BRK     = 8   // Expand heap
   SYS_KILL    = 11  // Send signal
   SYS_SIGNAL  = 21  // Install signal handler

**Example Test (signal_test.c):**

.. code-block:: c

   // Install signal handler
   signal(SIGUSR1, sigusr1_handler);
   
   // Send signal to self
   int pid = getpid();
   kill(pid, SIGUSR1);
   
   // Handler should be called
   delay(DELAY_LONG);
   if (signal_received == 1) {
       print("✓ SIGUSR1 delivered successfully\n");
   }

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

Common Test Failures
~~~~~~~~~~~~~~~~~~~~

**VirtIO Timeout / ext2 Mount Failure**

Symptom:

.. code-block:: text

   [FAIL] Failed to mount ext2 filesystem
   virtio_blk_do_request: Timeout waiting for device

**Cause:** Missing QEMU flag ``-global virtio-mmio.force-legacy=false``

**Fix:** Always use this flag. Modern VirtIO mode is required for I/O operations.

**Correct QEMU command:**

.. code-block:: bash

   qemu-system-riscv64 \
     -machine virt \
     -m 128M \
     -nographic \
     -serial mon:stdio \
     -bios none \
     -kernel build/thunderos.elf \
     -global virtio-mmio.force-legacy=false \
     -drive file=build/fs.img,if=none,format=raw,id=hd0 \
     -device virtio-blk-device,drive=hd0

---

**Unit Tests Fail at Boot**

Symptom: Tests marked as FAIL during kernel initialization

**Debugging steps:**

1. Check which test failed:

   .. code-block:: text

      ✗ FAIL: PMM allocation
   
2. Enable debug output in test file (``tests/unit/test_*.c``)

3. Review test expectations vs actual behavior

4. Use ``kprintf()`` to trace execution

---

**Memory Allocation Failures**

Symptom:

.. code-block:: text

   [ERROR] kmalloc failed: Out of memory
   ✗ FAIL: kmalloc basic

**Possible causes:**

- Physical memory exhausted (increase ``-m 128M`` in QEMU)
- Memory leak in previous test
- Bitmap corruption in PMM

**Debug approach:**

.. code-block:: c

   kprintf("[DEBUG] PMM: %d pages free\n", pmm_get_free_pages());
   kprintf("[DEBUG] Requesting %d bytes\n", size);

---

**Test Script Hangs**

Symptom: Test script doesn't complete, QEMU process hangs

**Causes:**

1. Infinite loop in kernel code
2. Deadlock in scheduler
3. Waiting for I/O that never completes

**Fix:**

- Add timeout to test script (already included in ``run_all_tests.sh``)
- Use ``timeout 30s qemu-system-riscv64 ...`` for manual runs
- Check for missing ``schedule()`` calls in blocking operations

Manual termination:

.. code-block:: bash

   # Press Ctrl+C or kill QEMU
   ps aux | grep qemu
   kill <PID>

---

**Shell Doesn't Appear**

Symptom: Kernel boots but no ``thunderos>`` prompt

**Checks:**

1. Verify shell process created:

   .. code-block:: text

      [OK] Created shell process (PID 5)

2. Check if shell is scheduled:

   .. code-block:: c

      // In scheduler.c
      kprintf("[DEBUG] Switching to PID %d\n", next->pid);

3. Verify UART is working:

   .. code-block:: c

      kprintf("Test output\n");  // Should appear before shell

---

**File Operations Fail**

Symptom: ``cat`` or ``ls`` commands return errors

**Debugging:**

1. Check filesystem mounted:

   .. code-block:: text

      [OK] ext2 filesystem mounted at /

2. Verify test files exist in filesystem image:

   .. code-block:: bash

      # View filesystem contents
      debugfs build/fs.img -R 'ls -l'

3. Check VFS error codes:

   .. code-block:: text

      thunderos> cat /missing.txt
      cat: /missing.txt: ENOENT (No such file or directory)

4. Review ext2 error handling (see ``docs/source/internals/ext2_filesystem.rst``)

---

Build and Environment Issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Error:** Cross-compiler not found

**Solution:** Install or add RISC-V toolchain to PATH:

.. code-block:: bash

   # Check toolchain
   which riscv64-unknown-elf-gcc
   
   # If not found, add to PATH
   export PATH=$PATH:/opt/riscv/bin

**Error:** QEMU not found

**Solution:** ThunderOS requires QEMU 10.1.2+ with RISC-V support:

.. code-block:: bash

   # Check version
   qemu-system-riscv64 --version
   
   # Build from source if needed
   wget https://download.qemu.org/qemu-10.1.2.tar.xz
   tar xJf qemu-10.1.2.tar.xz
   cd qemu-10.1.2
   ./configure --target-list=riscv64-softmmu
   make -j$(nproc)
   sudo make install

**Error:** ``make: command not found``

**Solution:** Ensure you're in the ThunderOS directory:

.. code-block:: bash

   cd /workspace
   make clean && make

Continuous Integration
----------------------

Automated Testing in CI/CD
~~~~~~~~~~~~~~~~~~~~~~~~~~~

ThunderOS uses GitHub Actions for automated testing. The workflow runs on:

- Push to ``main``, ``dev/*``, ``feature/*``, ``release/*`` branches
- Pull requests to ``main`` and ``dev/*`` branches

**CI Workflow (``.github/workflows/ci.yml``):**

.. code-block:: yaml

   name: ThunderOS CI
   
   on:
     push:
       branches: [main, dev/*, feature/*, release/*]
     pull_request:
       branches: [main, dev/*]
   
   jobs:
     build-and-test:
       runs-on: ubuntu-latest
       steps:
         - name: Checkout code
           uses: actions/checkout@v3
         
         - name: Build kernel
           run: make clean && make
         
         - name: Run tests
           run: make test
         
         - name: Upload test artifacts
           uses: actions/upload-artifact@v3
           with:
             name: test-results
             path: tests/outputs/

**Running Tests Locally:**

.. code-block:: bash

   # Same command as CI
   make test
   
   # Or run scripts directly
   tests/scripts/run_all_tests.sh

**Expected CI Output:**

.. code-block:: text

   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     Running ThunderOS Test Suite
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   
   [*] Running boot tests...
   ✓ Boot test passed
   
   [*] Running integration tests...
   ✓ Integration test passed
   
   [*] Running user mode tests...
   ✓ User mode test passed
   
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     Test Suite Complete: 3/3 passed
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Test artifacts are uploaded for debugging failed builds.
