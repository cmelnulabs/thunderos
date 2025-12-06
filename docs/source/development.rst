Development Guide
=================


Building
--------

Prerequisites
~~~~~~~~~~~~~

**Option 1: Using Docker (Recommended)**

The easiest way to build ThunderOS is using Docker, which provides a consistent build environment:

.. code-block:: bash

   # Build the Docker image
   docker build -t thunderos-build .
   
   # Build ThunderOS in Docker
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build make all
   
   # Run in QEMU via Docker
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build make qemu

**Option 2: Native Installation (Ubuntu/Debian)**

.. code-block:: bash

   # QEMU (for emulation)
   sudo apt-get install qemu-system-misc
   
   # Build tools
   sudo apt-get install build-essential make wget
   
   # RISC-V toolchain (manual installation required)
   # Download from SiFive or build from source
   wget https://static.dev.sifive.com/dev-tools/freedom-tools/v2020.12/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz
   tar -xzf riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz
   sudo mv riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14 /opt/riscv
   export PATH="/opt/riscv/bin:$PATH"  # Add to ~/.bashrc
   
   # Verify installation
   riscv64-unknown-elf-gcc --version
   qemu-system-riscv64 --version

**Option 3: Using Build Scripts (After Setup)**

Once prerequisites are installed:

.. code-block:: bash

   ./build_os.sh           # Build the kernel
   ./build_userland.sh     # Build user programs
   make test               # Run all tests
   ./build_docs.sh         # Build documentation

Compilation
~~~~~~~~~~~

**Using Build Scripts (Recommended)**

.. code-block:: bash

   ./build_os.sh       # Clean build of the kernel
   ./test_qemu.sh      # Build and run in QEMU
   ./build_docs.sh     # Build Sphinx documentation

**Using Make Directly**

.. code-block:: bash

   # Full build
   make all
   
   # Clean build
   make clean && make all
   
   # Run in QEMU
   make qemu
   
   # Debug with GDB
   make debug

Testing
-------

Manual Testing
~~~~~~~~~~~~~~

**Quick Test with Script**

.. code-block:: bash

   make test                          # Run full test suite
   tests/scripts/test_boot.sh         # Boot and unit tests
   tests/scripts/test_integration.sh  # VirtIO, ext2, shell tests
   tests/scripts/test_user_mode.sh    # User-space program tests

**Manual Steps**

1. Build kernel: ``./build_os.sh`` or ``make all``
2. Run in QEMU: ``make qemu``
3. Verify boot messages appear
4. Test each feature manually
5. Press ``Ctrl+A`` then ``X`` to exit QEMU

Automated Testing
~~~~~~~~~~~~~~~~~

ThunderOS includes comprehensive automated CI/CD testing via GitHub Actions. See ``.github/workflows/ci.yml`` for the complete test suite.

The CI pipeline:

1. Builds kernel in Docker
2. Runs QEMU boot test
3. Executes automated integration tests:
   
   - ``test_boot.sh`` - Kernel boot and unit tests
   - ``test_integration.sh`` - VirtIO, ext2, shell functionality
   - ``test_user_mode.sh`` - User-mode programs and syscalls

4. Verifies boot messages and initialization
5. Checks for build warnings
6. Runs unit tests (if available)
7. Uploads test artifacts

To run similar tests locally:

.. code-block:: bash

   # Build and test (mimics CI)
   docker build -t thunderos-build .
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build make clean && make
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build make test


Debugging
---------

Memory Debugging
~~~~~~~~~~~~~~~~

ThunderOS supports comprehensive memory debugging using existing tools with no code changes required.

Quick Start
^^^^^^^^^^^

.. code-block:: bash

   # Enable comprehensive QEMU memory tracing (built-in target)
   make qemu-debug-mem

   # Or use GDB for interactive debugging
   make debug
   make gdb  # In another terminal

QEMU Built-in Tracing
^^^^^^^^^^^^^^^^^^^^^^

QEMU has powerful built-in tracing and logging capabilities that require zero code changes.

**Basic Memory Logging**

Enable guest error and unimplemented feature logging:

.. code-block:: bash

   qemu-system-riscv64 \
       -machine virt -m 128M \
       -nographic -serial mon:stdio \
       -bios none \
       -kernel build/thunderos.elf \
       -global virtio-mmio.force-legacy=false \
       -drive file=build/fs.img,if=none,format=raw,id=hd0 \
       -device virtio-blk-device,drive=hd0 \
       -d guest_errors,unimp \
       -D qemu.log

**Flags:**

- ``-d guest_errors`` - Log guest errors (page faults, invalid memory access)
- ``-d unimp`` - Log unimplemented features accessed by guest
- ``-D qemu.log`` - Write logs to file instead of stderr

**All Available Debug Options**

List all QEMU debug options:

.. code-block:: bash

   qemu-system-riscv64 -d help

**Useful options for memory debugging:**

================= ====================================================
Flag              Description
================= ====================================================
``-d in_asm``     Show generated assembly (TCG)
``-d out_asm``    Show host assembly output
``-d int``        Log interrupts and exceptions
``-d mmu``        Log MMU-related activity (page tables, TLB)
``-d page``       Log page allocations
``-d guest_errors`` Log guest OS errors
``-d unimp``      Log unimplemented device/feature access
``-d cpu``        Log CPU state changes
``-d exec``       Show instruction execution trace
================= ====================================================

**Memory Access Tracing**

For detailed memory access tracing, use QEMU trace events:

.. code-block:: bash

   # List available trace events
   qemu-system-riscv64 -trace help

   # Enable memory-related traces
   qemu-system-riscv64 \
       ... (kernel args) ... \
       -trace 'memory_region_*' \
       -trace 'load_*' \
       -trace 'store_*' \
       -D memory_trace.log

**Common trace patterns:**

- ``memory_region_*`` - Memory region operations
- ``load_*`` - Load operations from memory
- ``store_*`` - Store operations to memory
- ``dma_*`` - DMA operations

**Example: Debug Memory Leak**

1. Run kernel with logging:

   .. code-block:: bash

      qemu-system-riscv64 ... -d guest_errors,page -D memory.log

2. Check log for page allocations:

   .. code-block:: bash

      grep "page" memory.log

3. Look for unfreed pages or growing memory usage

GDB Memory Debugging
^^^^^^^^^^^^^^^^^^^^

GDB provides interactive memory inspection and watchpoints.

**Start GDB Session**

Terminal 1 - Start QEMU with GDB server:

.. code-block:: bash

   qemu-system-riscv64 \
       -machine virt -m 128M \
       -nographic -serial mon:stdio \
       -bios none \
       -kernel build/thunderos.elf \
       -global virtio-mmio.force-legacy=false \
       -drive file=build/fs.img,if=none,format=raw,id=hd0 \
       -device virtio-blk-device,drive=hd0 \
       -s -S

**Flags:**

- ``-s`` - Start GDB server on ``localhost:1234``
- ``-S`` - Freeze CPU at startup (wait for GDB)

Terminal 2 - Connect GDB:

.. code-block:: bash

   riscv64-unknown-elf-gdb build/thunderos.elf

   (gdb) target remote :1234
   (gdb) continue

Or use the Makefile target:

.. code-block:: bash

   # Terminal 1
   make debug

   # Terminal 2
   make gdb

**Memory Inspection Commands**

.. code-block:: gdb

   # Examine memory at address
   (gdb) x/10x 0x80000000        # 10 hex words
   (gdb) x/10i 0x80000000        # 10 instructions
   (gdb) x/10s 0x80000000        # 10 strings
   (gdb) x/10c 0x80000000        # 10 characters

   # Watch memory location
   (gdb) watch *(int*)0x80001000  # Break on write
   (gdb) rwatch *(int*)0x80001000 # Break on read
   (gdb) awatch *(int*)0x80001000 # Break on access

   # Examine variables
   (gdb) print g_alloc_count
   (gdb) print/x g_process_table[0]

   # Memory regions
   (gdb) info mem               # Show memory regions
   (gdb) info proc mappings     # Show process memory map (if supported)

**Find Memory Leaks with GDB**

.. code-block:: gdb

   # Set breakpoint at kmalloc
   (gdb) break kmalloc
   (gdb) commands
     > silent
     > printf "kmalloc(%d) = ", $a0
     > finish
     > printf "%p\n", $a0
     > continue
     > end

   # Set breakpoint at kfree
   (gdb) break kfree
   (gdb) commands
     > silent
     > printf "kfree(%p)\n", $a0
     > continue
     > end

   # Run and analyze alloc/free pairs
   (gdb) continue

**GDB Script for Memory Tracking**

Create ``gdb_memory.py``:

.. code-block:: python

   import gdb

   class TrackAllocations(gdb.Command):
       def __init__(self):
           super(TrackAllocations, self).__init__("track-alloc", gdb.COMMAND_USER)
           self.allocations = {}
       
       def invoke(self, arg, from_tty):
           # Set breakpoint on kmalloc
           bp_malloc = gdb.Breakpoint("kmalloc")
           bp_malloc.silent = True
           bp_malloc.commands = "python TrackAllocations.on_malloc()"
           
           # Set breakpoint on kfree
           bp_free = gdb.Breakpoint("kfree")
           bp_free.silent = True
           bp_free.commands = "python TrackAllocations.on_free()"
           
           gdb.execute("continue")
       
       def on_malloc():
           size = gdb.parse_and_eval("$a0")
           gdb.execute("finish")
           addr = gdb.parse_and_eval("$a0")
           TrackAllocations.allocations[addr] = size
           print(f"Allocated {size} bytes at {addr}")
       
       def on_free():
           addr = gdb.parse_and_eval("$a0")
           if addr in TrackAllocations.allocations:
               del TrackAllocations.allocations[addr]
               print(f"Freed {addr}")
           else:
               print(f"WARNING: Double free or invalid free at {addr}")

   TrackAllocations()

Load in GDB:

.. code-block:: gdb

   (gdb) source gdb_memory.py
   (gdb) track-alloc

QEMU Monitor Commands
^^^^^^^^^^^^^^^^^^^^^^

Access QEMU monitor while kernel is running by pressing ``Ctrl-A C`` (then ``Ctrl-A C`` again to return to console).

**Useful Monitor Commands**

.. code-block:: text

   # Memory information
   (qemu) info mem              # Show page table mappings
   (qemu) info tlb              # Show TLB contents
   (qemu) info registers        # Show CPU registers

   # Memory operations
   (qemu) x /10x 0x80000000     # Examine physical memory
   (qemu) xp /10x 0x80000000    # Examine physical memory (explicit)

   # System state
   (qemu) info mtree            # Memory tree (all memory regions)
   (qemu) info qtree            # Device tree

   # Save/restore state
   (qemu) savevm snap1          # Save VM snapshot
   (qemu) loadvm snap1          # Load VM snapshot

   # Tracing control
   (qemu) trace-event memory_region_ops on
   (qemu) trace-event memory_region_ops off

**Memory Tree Analysis**

.. code-block:: text

   (qemu) info mtree

This shows all memory regions including:

- Physical RAM mapping
- MMIO device regions (UART, VirtIO, CLINT, PLIC)
- ROM regions
- Overlapping regions (potential bugs!)

Makefile Integration
^^^^^^^^^^^^^^^^^^^^

ThunderOS includes these built-in debug targets:

**Memory Debugging Target**

.. code-block:: bash

   # Comprehensive memory debugging with QEMU tracing
   make qemu-debug-mem

**What it does:**

- Logs: ``guest_errors,int,mmu,page``
- Output: ``memory_debug.log``
- Includes: Guest errors, interrupts, MMU operations, page allocations
- **Warning:** Generates large log files (100MB+ for 10 second runs)

**Flags enabled:**

- ``-d guest_errors`` - Invalid memory access, hardware violations
- ``-d int`` - All CPU exceptions and interrupts (syscalls, page faults, timers)
- ``-d mmu`` - TLB operations, page table walks, address translations
- ``-d page`` - Page allocation and deallocation tracking

**GDB Debugging Targets**

.. code-block:: bash

   # Terminal 1: Start QEMU with GDB server
   make debug

   # Terminal 2: Connect GDB client
   make gdb

**What they do:**

- ``make debug`` - Launches QEMU with ``-s -S`` (GDB server on port 1234, CPU frozen)
- ``make gdb`` - Connects riscv64-unknown-elf-gdb with split layout view

**Usage Examples**

.. code-block:: bash

   # Run memory debugging for 5 seconds
   timeout 5 make qemu-debug-mem

   # Analyze the log
   ls -lh memory_debug.log
   grep "page fault" memory_debug.log
   grep "riscv_cpu_do_interrupt" memory_debug.log | head -20

   # GDB debugging workflow
   make debug          # Terminal 1
   make gdb            # Terminal 2
   # In GDB:
   (gdb) break kmalloc
   (gdb) continue
   (gdb) backtrace

Debug References
^^^^^^^^^^^^^^^^

- `QEMU Documentation - System Emulation <https://www.qemu.org/docs/master/system/index.html>`_
- `QEMU Tracing Documentation <https://www.qemu.org/docs/master/devel/tracing.html>`_
- `GDB Manual <https://sourceware.org/gdb/current/onlinedocs/gdb/>`_
- `RISC-V GDB <https://github.com/riscv/riscv-gnu-toolchain>`_

Quick Reference
^^^^^^^^^^^^^^^

.. code-block:: text

   # QEMU Debug Flags
   -d guest_errors       Guest OS errors
   -d int                Interrupts/exceptions
   -d mmu                MMU operations
   -d page               Page allocations
   -D logfile.txt        Log output file

   # GDB Commands
   x/FMT ADDR           Examine memory
   watch EXPR           Break on write
   info mem             Memory regions
   backtrace            Call stack

   # QEMU Monitor (Ctrl-A C)
   info mem             Page tables
   info mtree           Memory tree
   x /FMT ADDR          Examine memory



Community
---------

Getting Help
~~~~~~~~~~~~

* Read the documentation first
* Check existing issues
* Ask in discussions

Reporting Bugs
~~~~~~~~~~~~~~

Include:

1. What you expected
2. What actually happened
3. Steps to reproduce
4. Your environment (QEMU version, etc.)

Feature Requests
~~~~~~~~~~~~~~~~

Open an issue with:

1. Use case
2. Proposed API
3. Implementation ideas (if any)

Code Quality
------------

For detailed code quality standards, style guidelines, and refactoring principles, see:

.. toctree::
   :maxdepth: 2

   development/code_quality

License
-------

ThunderOS is free software licensed under the GNU General Public License v3.0 (GPL v3).

This means you are free to use, study, modify, and distribute the software, provided that any distributed modifications are also licensed under GPL v3 and include source code.

See Also
--------

* :doc:`architecture` - System design
* :doc:`internals/index` - Implementation details
* :doc:`api` - API reference


