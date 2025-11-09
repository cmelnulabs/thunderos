# ThunderOS Test Suite

This directory contains automated test scripts for ThunderOS functionality.

## Test Scripts

### Integration Tests
- **`test_qemu.sh`** - Basic QEMU boot and kernel functionality test
- **`test_syscalls.sh`** - Comprehensive syscall testing (6 automated checks)
- **`test_user_mode.sh`** - User-mode process execution and privilege separation
- **`test_user_quick.sh`** - Fast user-mode validation (4 checks)

### Unit Tests
- **`test_trap.c`** - Trap handler unit tests
- **`test_timer.c`** - Timer interrupt unit tests
- **`test_paging.c`** - Memory paging unit tests

## Running Tests

### Individual Scripts
```bash
# From the tests/ directory
./test_user_quick.sh    # Fast validation
./test_syscalls.sh      # Full syscall test
./test_user_mode.sh     # User-mode comprehensive test
./test_qemu.sh          # Basic QEMU functionality

# From the root directory
tests/test_user_quick.sh
```

### Unit Tests
```bash
# Build and run unit tests
cd tests
make
make run-test-trap
make run-test-timer
```

## Test Output

All test scripts produce colored output with:
- `[✓]` Green for passed tests
- `[✗]` Red for failed tests
- `[*]` Yellow for informational messages

Test results are saved to `tests/*.txt` files for analysis.

## CI Integration

These tests are automatically run in GitHub Actions CI:
- Build verification
- QEMU boot testing
- Automated functional validation
- Test output artifacts uploaded

## Test Categories

### Critical Tests (must pass)
- Kernel initialization
- Process creation
- User-mode processes running

### Optional Tests (nice to have)
- System calls working
- Scheduler operational
- Memory management
- Exception handling

## Adding New Tests

1. Follow the existing format with colored output
2. Save output to `tests/your_test_output.txt`
3. Update `.github/workflows/ci.yml` to include new test
4. Update artifact upload list for test logs