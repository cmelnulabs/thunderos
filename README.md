# ThunderOS

A RISC-V operating system focused on AI acceleration and educational use.

## Current Status

**Version 0.2.0 - "User Space"** 🎯 In Development

- ✅ Kernel infrastructure complete
- ✅ User mode (U-mode) working
- ✅ System calls implemented (13 syscalls)
- ✅ Syscall testing framework ready
- ✅ User-space hello world program

See [CHANGELOG.md](CHANGELOG.md) for complete feature list and [ROADMAP.md](ROADMAP.md) for future plans.

## Quick Start

### Building
```bash
make clean && make
```

### Running in QEMU
```bash
make qemu
```

### Automated Testing
```bash
# Run comprehensive syscall tests
tests/test_syscalls.sh

# Run quick user-mode validation
tests/test_user_quick.sh

# Run all tests (see tests/README.md)
cd tests && ./test_*.sh
```

### Debugging
```bash
make debug
# In another terminal:
riscv64-unknown-elf-gdb build/thunderos.elf
(gdb) target remote :1234
```

## Documentation

Full technical documentation is available in Sphinx format:

```bash
cd docs
make html
# Open docs/build/html/index.html in browser
```

## Project Structure
```
boot/                - Bootloader and early initialization
kernel/              - Kernel core
  arch/riscv64/      - RISC-V architecture-specific code
    drivers/         - RISC-V HAL implementations (UART, timer, etc.)
    interrupt/       - Trap/interrupt handling
  core/              - Portable kernel core (process, scheduler, panic)
  mm/                - Memory management (PMM, kmalloc, paging)
include/             - Header files
  hal/               - Hardware Abstraction Layer interfaces
  kernel/            - Kernel subsystem headers
  arch/              - Architecture-specific headers
docs/                - Sphinx documentation
tests/               - Test framework and test cases
build/               - Build output
```

## Development

See [ROADMAP.md](ROADMAP.md) for the development roadmap from v0.1 through v2.0.

See [docs/source/development/code_quality.rst](docs/source/development/code_quality.rst) for coding standards.

## Testing

### Test Framework
The project includes an automated test suite:

```bash
./test_syscalls.sh
```

This script:
- Compiles the kernel
- Runs QEMU with 5-second timeout
- Captures output to `/tmp/thunderos_test_output.txt`
- Validates key system components:
  - ✓ Kernel initialization
  - ✓ Process creation
  - ✓ Scheduler functionality
  - ✓ User process creation
  - ✓ Syscall output (sys_write)

### User-Space Programs

Located in `tests/`:
- **user_hello.c** - Demonstrates user-space syscalls:
  - Reads current process ID via SYS_GETPID
  - Outputs to console via SYS_WRITE
  - Gets system time via SYS_GETTIME
  - Yields CPU via SYS_YIELD
  - Exits cleanly via SYS_EXIT

Compile with cross-compiler:
```bash
riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d -nostdlib \
    -Iinclude tests/user_hello.c -o tests/user_hello.o
```

## Platform Support

- **QEMU virt machine**: Tested and working ✓

## Dependencies

- **Toolchain**: riscv64-unknown-elf-gcc (GCC for RISC-V)
- **Emulator**: QEMU 5.0+ with RISC-V support
- **Firmware**: OpenSBI (provided by QEMU)
- **Documentation** (optional): Sphinx 4.0+

## License

See [LICENSE](LICENSE) file for details.
