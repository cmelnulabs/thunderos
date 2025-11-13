# ThunderOS Test Suite# ThunderOS Test Suite



Automated test suite for ThunderOS v0.5.0+Automated test suite for ThunderOS v0.5.0+



## Directory Structure## Test Organization



```### Built-in Kernel Tests (run automatically on boot)

tests/These tests are compiled into the kernel and run during `kernel_main()`:

├── unit/                      # Built-in kernel tests (C)- **Memory Management** (`test_memory_mgmt.c`) - DMA, kmalloc, paging

│   ├── test_memory_mgmt.c    # Memory, DMA, paging tests- **ELF Loader** (`test_elf.c`) - ELF parsing and validation

│   └── test_elf.c            # ELF loader tests

├── scripts/                   # Integration test scripts### Integration Tests (shell scripts)

│   ├── test_boot.sh          # Quick boot test (~5s)Automated tests that boot QEMU and analyze output:

│   ├── test_integration.sh   # Full system test (~8s)- **`test_boot.sh`** - Basic kernel boot and initialization

│   └── run_all_tests.sh      # Master test runner- **`test_integration.sh`** - Full system test (VirtIO, ext2, shell)

├── framework/                 # Test framework

│   ├── kunit.c               # Test assertions### Framework

│   └── kunit.h               # Test macros- **`framework/kunit.{c,h}`** - Test assertion framework

└── outputs/                   # Test output files (generated)

```## Running Tests



## Test Organization### Quick Test (30 seconds)

```bash

### Built-in Kernel Tests (Unit Tests)cd tests

Tests compiled into the kernel and run automatically during `kernel_main()`:./test_boot.sh

- **Memory Management** (`unit/test_memory_mgmt.c`) - DMA allocation, kmalloc, paging```

- **ELF Loader** (`unit/test_elf.c`) - ELF parsing and validation

### Full Integration Test (60 seconds)

Output is visible in QEMU console during boot.```bash

cd tests

### Integration Tests (Shell Scripts)./test_integration.sh

Automated tests that boot QEMU and analyze output:```

- **`scripts/test_boot.sh`** - Basic kernel boot and initialization (6 checks)

- **`scripts/test_integration.sh`** - Full system test: VirtIO, ext2, shell, ELF (7 checks)### All Tests

- **`scripts/run_all_tests.sh`** - Runs all tests sequentially```bash

cd tests

## Running Tests./run_all_tests.sh

```

### Quick Boot Test (5 seconds)

```bash### From Project Root

cd tests/scripts```bash

./test_boot.shmake test

``````



### Full Integration Test (8 seconds)## Test Output Format

```bash

cd tests/scriptsAll tests follow this standard format:

./test_integration.sh

``````

========================================

### All Tests (13 seconds)  Test Name

```bash========================================

cd tests/scripts

./run_all_tests.sh[TEST] Test case description

```  [PASS] Specific check passed

  [FAIL] Specific check failed

### From Project Root  [INFO] Informational message

```bash

make test========================================

```Summary: X/Y tests passed

========================================

## Test Output Format```



All tests follow this standard format:Exit codes:

- `0` - All tests passed

```- `1` - One or more tests failed

========================================- `124` - Timeout (usually expected for QEMU tests)

  Test Name

========================================## CI Integration



[TEST] Test case descriptionGitHub Actions runs:

  [PASS] Specific check passed ✓1. `test_boot.sh` - Verify kernel boots

  [FAIL] Specific check failed ✗2. `test_integration.sh` - Verify full system functionality

  [INFO] Informational message

Test artifacts are uploaded for failed runs.

========================================

Summary: X/Y tests passed## Adding New Tests

========================================

```1. **For kernel tests:** Create `test_yourfeature.c` and call it from `kernel/main.c`

2. **For integration tests:** Add checks to `test_integration.sh`

**Exit codes:**3. Update this README

- `0` - All tests passed4. Update `.github/workflows/ci.yml` if needed

- `1` - One or more tests failed

- `124` - Timeout (usually expected for QEMU tests)## Test Files



**Output files:** Saved to `tests/outputs/` directory:### Keep (Active Tests)

- `boot_test_output.txt` - Boot test QEMU output- ✅ `test_memory_mgmt.c` - Used by kernel

- `integration_test_output.txt` - Integration test QEMU output- ✅ `test_elf.c` - Used by kernel  

- ✅ `framework/kunit.{c,h}` - Test framework

## CI Integration

### Remove (Obsolete/Redundant)

GitHub Actions runs:- ❌ `test_errno.c` - Tested by integration tests

1. `make test` - Runs complete test suite- ❌ `test_ext2.c` - Tested by integration tests

2. Test artifacts uploaded on failure: `boot_test_output.txt`, `integration_test_output.txt`- ❌ `test_syscalls.c` - Tested by integration tests

- ❌ `test_vfs.c` - Tested by integration tests

Exit code determines CI pass/fail (0 = success, 1 = failure).- ❌ `test_virtio_blk.c` - Tested by integration tests

- ❌ `test_paging.c` - Covered by test_memory_mgmt.c

## Adding New Tests- ❌ `test_trap.c` - No longer needed

- ❌ `test_timer.c` - No longer needed

### Adding Built-in Tests- ❌ `test_qemu.sh` - Replaced by test_boot.sh

1. Create `tests/unit/test_yourfeature.c`- ❌ `test_syscalls.sh` - Replaced by test_integration.sh

2. Implement using `KUNIT_ASSERT_*` macros from `framework/kunit.h`- ❌ `test_user_mode.sh` - Replaced by test_integration.sh

3. Call test function from `kernel/main.c` during boot- ❌ `test_user_quick.sh` - Replaced by test_boot.sh

4. Update this README- ❌ `test_exec_automated.sh` - Manual test, not automated

- ❌ `test_program_exec.sh` - Manual test, not automated

Example:- ❌ `test_shell_commands.sh` - Manual test, not automated

```c- ❌ `test_virtio_driver.sh` - Manual test, not automated

#include "framework/kunit.h"- ❌ `Makefile` - Standalone tests no longer used

- ❌ `user_*.c` - Test programs moved to userland/

void test_your_feature(void) {
    kprintf("[TEST] Your Feature\n");
    KUNIT_ASSERT_EQ(1 + 1, 2, "Math works");
    kprintf("  [PASS] All checks passed\n");
}
```

### Adding Integration Tests
1. Add checks to `scripts/test_integration.sh`
2. Use `check_output` function with grep pattern
3. Update test counter
4. Update this README

Example:
```bash
check_output "Your Feature" "Expected output pattern"
```

## Test Suite Reorganization (2025-11-13)

### What Was Removed

**18 obsolete test files deleted:**

**Shell Scripts (8):**
- `test_qemu.sh` → Replaced by `test_boot.sh`
- `test_syscalls.sh` → Replaced by `test_integration.sh`
- `test_user_mode.sh` → Replaced by `test_integration.sh`
- `test_user_quick.sh` → Replaced by `test_boot.sh`
- `test_exec_automated.sh` → Manual test, not CI-friendly
- `test_program_exec.sh` → Manual test, not CI-friendly
- `test_shell_commands.sh` → Manual test, not CI-friendly
- `test_virtio_driver.sh` → Manual test, not CI-friendly

**C Test Files (10):**
- `test_errno.c` → Now tested through integration tests
- `test_ext2.c` → Now tested through integration tests
- `test_syscalls.c` → Now tested through integration tests
- `test_vfs.c` → Now tested through integration tests
- `test_virtio_blk.c` → Now tested through integration tests
- `test_paging.c` → Covered by `test_memory_mgmt.c`
- `test_trap.c` → No longer needed
- `test_timer.c` → No longer needed
- `user_hello.c` → Moved to `userland/hello.c`
- `user_test.c` → Obsolete

**Build Infrastructure:**
- `tests/Makefile` → Tests now built-in to kernel

**Assembly Tests:**
- `user_exception_test.{S,c}` → No longer maintained

### Migration Guide

**Old commands → New commands:**

| Old | New |
|-----|-----|
| `./test_qemu.sh` | `cd scripts && ./test_boot.sh` |
| `./test_syscalls.sh` | `cd scripts && ./test_integration.sh` |
| `./test_user_mode.sh` | `cd scripts && ./test_integration.sh` |
| `./test_user_quick.sh` | `cd scripts && ./test_boot.sh` |
| `cd tests && make` | *Not needed - tests built into kernel* |

**From project root:**
```bash
make test  # Runs complete test suite
```

### Benefits of Reorganization

1. **72% reduction** in test files (18 → 5 active tests)
2. **Consistency**: All tests follow same format
3. **Speed**: Reduced from 4 separate test runs to 2 unified tests
4. **Maintainability**: Less code duplication
5. **CI-friendly**: All tests automated, no manual interaction
6. **Clear output**: Standardized pass/fail indicators with colors
7. **Better organization**: Tests grouped by type (unit vs integration)

### Statistics

| Category | Count Before | Count After | Reduction |
|----------|--------------|-------------|-----------|
| Shell scripts | 8 | 3 | 63% |
| C test files | 10 | 2 | 80% |
| Total test files | 18 | 5 | 72% |

**Result**: Cleaner, faster, more maintainable test suite with better coverage.

## Test Framework (kunit)

The test framework provides assertion macros for built-in tests:

- `KUNIT_ASSERT_TRUE(cond, msg)` - Assert condition is true
- `KUNIT_ASSERT_FALSE(cond, msg)` - Assert condition is false
- `KUNIT_ASSERT_EQ(a, b, msg)` - Assert a equals b
- `KUNIT_ASSERT_NE(a, b, msg)` - Assert a not equals b
- `KUNIT_ASSERT_NOT_NULL(ptr, msg)` - Assert pointer is not NULL
- `KUNIT_ASSERT_NULL(ptr, msg)` - Assert pointer is NULL

All assertions print formatted output with pass/fail status.

## Troubleshooting

### Tests Fail with "Timeout"
- **Cause**: QEMU not responding within expected time
- **Solution**: Check if kernel panicked or hung - look at QEMU output in `outputs/`

### Tests Fail with "Failed to mount ext2"
- **Cause**: Missing `-global virtio-mmio.force-legacy=false` flag
- **Solution**: Verify test scripts use correct QEMU invocation

### Build Errors About Missing Test Files
- **Cause**: Makefile references deleted test files
- **Solution**: Verify `KERNEL_C_SOURCES` only includes `unit/test_memory_mgmt.c` and `unit/test_elf.c`

### CI Fails But Local Tests Pass
- **Cause**: Path differences or missing files
- **Solution**: Check CI logs for specific errors, verify all files committed

## Contributing

When adding tests:
1. Follow the standard output format
2. Update test counters and summaries
3. Ensure tests are deterministic and reproducible
4. Add documentation to this README
5. Test locally before submitting PR

See `CONTRIBUTING.md` for general contribution guidelines.
