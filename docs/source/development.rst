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

   ./build_os.sh       # Build the kernel
   tests/test_qemu.sh  # Build and test in QEMU
   ./build_docs.sh     # Build documentation

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

   tests/test_user_quick.sh    # Fast validation (3 seconds)
   tests/test_syscalls.sh      # Comprehensive syscall testing
   tests/test_user_mode.sh     # User-mode privilege testing
   tests/test_qemu.sh          # Basic QEMU functionality

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
3. Executes 4 automated integration tests:
   
   - ``test_qemu.sh`` - Basic functionality
   - ``test_syscalls.sh`` - Syscall validation
   - ``test_user_mode.sh`` - User-mode testing
   - ``test_user_quick.sh`` - Fast validation

4. Verifies boot messages and initialization
5. Checks for build warnings
6. Runs unit tests (if available)
7. Uploads test artifacts

To run similar tests locally:

.. code-block:: bash

   # Build and test (mimics CI)
   docker build -t thunderos-build .
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build make clean && make
   docker run --rm -v $(pwd):/workspace -w /workspace thunderos-build bash -c "cd tests && ./test_user_quick.sh"


Debugging
---------

QEMU + GDB
~~~~~~~~~~

Terminal 1:

.. code-block:: bash

   make debug
   # QEMU waits for GDB connection

Terminal 2:

.. code-block:: bash

   riscv64-unknown-elf-gdb build/thunderos.elf
   (gdb) target remote :1234
   (gdb) break kernel_main
   (gdb) continue



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


