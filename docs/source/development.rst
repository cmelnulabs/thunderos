Development Guide
=================


Building
--------

Prerequisites
~~~~~~~~~~~~~

.. code-block:: bash

   # RISC-V toolchain
   apt-get install gcc-riscv64-unknown-elf
   
   # QEMU
   apt-get install qemu-system-riscv64
   
   # Build tools
   apt-get install make

Compilation
~~~~~~~~~~~

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

1. Build kernel: ``make all``
2. Run in QEMU: ``make qemu``
3. Verify boot messages appear
4. Test each feature manually

Automated Testing
~~~~~~~~~~~~~~~~~

Create test scripts:

.. code-block:: bash

   #!/bin/bash
   # tests/boot_test.sh
   
   timeout 5 make qemu > output.txt 2>&1
   
   if grep -q "ThunderOS" output.txt; then
       echo "PASS: Kernel boots"
   else
       echo "FAIL: Kernel doesn't boot"
       exit 1
   fi

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


