# ThunderOS Test Suite

Automated test suite for ThunderOS v0.7.0+

## Directory Structure

```
tests/
├── unit/                      # Built-in kernel tests (C)
│   ├── test_memory_mgmt.c    # Memory, DMA, paging tests
│   ├── test_elf.c            # ELF loader tests
│   ├── test_memory_isolation.c # Memory isolation tests
│   └── test_vterm.c          # Virtual terminal tests
├── scripts/                   # Automated test scripts
│   ├── test_kernel.sh        # Comprehensive kernel test
│   ├── test_boot.sh          # Quick boot test
│   ├── test_integration.sh   # Integration test
│   └── run_all_tests.sh      # Master test runner
├── framework/                 # Test framework
│   ├── kunit.c               # Test assertions
│   └── kunit.h               # Test macros
└── outputs/                   # Test output files (generated)
```

## Running Tests

### Using Make (Recommended)

```bash
# Full test suite
make test

# Quick boot test only
make test-quick
```

### Using Scripts Directly

```bash
# Comprehensive kernel functionality test
./tests/scripts/test_kernel.sh

# Quick boot test
./tests/scripts/test_boot.sh

# Integration test
./tests/scripts/test_integration.sh

# All tests
./tests/scripts/run_all_tests.sh
```

## Test Types

### 1. Built-in Kernel Tests

Unit tests compiled into the kernel that run automatically during boot:

- **Memory Management** (`test_memory_mgmt.c`)
  - DMA allocation and alignment
  - kmalloc/kfree operations
  - Paging and virtual memory

- **ELF Loader** (`test_elf.c`)
  - ELF header validation
  - Program header parsing

- **Memory Isolation** (`test_memory_isolation.c`)
  - User/kernel address space separation

These tests output results to the console and are verified by `test_kernel.sh`.

### 2. Automated Test Scripts

Shell scripts that run QEMU non-interactively and verify output:

- **`test_kernel.sh`** - Comprehensive test (17 checks)
  - Kernel boot and initialization
  - All subsystem initialization
  - Built-in test results
  - VirtIO and ext2 mounting
  - User-mode shell launch

- **`test_boot.sh`** - Quick sanity test (6 checks)
  - Kernel banner
  - UART initialization
  - Memory management
  - Trap handler

- **`test_integration.sh`** - Integration test (7 checks)
  - VirtIO block device
  - ext2 filesystem
  - Shell startup

## Test Philosophy

All tests are **non-interactive**. They:

1. Build the kernel and userland
2. Create an ext2 filesystem image
3. Run QEMU with a timeout (no stdin)
4. Capture and analyze output
5. Pattern-match for expected strings

This allows all tests to run in CI without user input.

## Output Files

Test output is saved in `tests/outputs/`:

- `kernel_test_output.txt` - Output from `test_kernel.sh`
- `boot_test_output.txt` - Output from `test_boot.sh`
- `integration_test_output.txt` - Output from `test_integration.sh`

## Adding New Tests

### Adding a Built-in Test

1. Create a new file in `tests/unit/`
2. Include `tests/framework/kunit.h`
3. Write test functions using `KUNIT_ASSERT_*` macros
4. Call your tests from `kernel_main()` in `kernel/main.c`

### Adding an Automated Check

1. Edit `tests/scripts/test_kernel.sh`
2. Add a new `grep` pattern to check for expected output
3. Update the test count and summary

## Requirements

- QEMU 10.1.2+ with riscv64 support
- RISC-V GCC toolchain
- e2fsprogs (`mkfs.ext2`)

## Continuous Integration

For CI/CD pipelines:

```bash
# Run full test suite (exit code 0 = success)
make test

# Or for faster CI:
make test-quick
```

Tests are designed to complete within 30 seconds and require no user interaction.
