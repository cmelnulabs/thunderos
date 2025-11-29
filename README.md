# ThunderOS

A RISC-V operating system focused on AI acceleration and educational use.

## Current Status

**Version 0.7.0 - "Virtual Terminals"** 🎯 Released!

- ✅ **v0.7.0 Released** - Virtual terminals and VirtIO GPU
- ✅ 6 virtual terminals (VT1-VT6) with ESC+1-6 switching
- ✅ Multiple independent shell instances (VT1 and VT2)
- ✅ VirtIO GPU 2D driver for graphics support
- ✅ Per-terminal input buffers and screen state
- ✅ New utilities: ps, uname, uptime, whoami, tty
- ✅ 35 system calls implemented
- 🚧 **Next**: Framebuffer console, job control (v0.8.0)

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

The OS will automatically build the filesystem image and start QEMU with VirtIO block device support.

### Automated Testing
```bash
# Run all tests
make test

# Individual test scripts
cd tests/scripts
./test_boot.sh          # Quick boot validation
./test_kernel.sh        # Comprehensive kernel test
./test_integration.sh   # Full integration tests
./run_all_tests.sh      # Run all test suites
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
  core/              - Portable kernel core (process, scheduler, shell, ELF loader)
  drivers/           - Device drivers (VirtIO block, etc.)
  fs/                - Filesystem implementations (VFS, ext2)
  mm/                - Memory management (PMM, kmalloc, paging, DMA)
include/             - Header files
  hal/               - Hardware Abstraction Layer interfaces
  kernel/            - Kernel subsystem headers
  fs/                - Filesystem headers (VFS, ext2)
  arch/              - Architecture-specific headers (barriers, etc.)
  mm/                - Memory management headers (DMA, paging)
docs/                - Sphinx documentation
tests/               - Test framework and test cases
userland/            - User-space programs
build/               - Build output
```

## Development

See [ROADMAP.md](ROADMAP.md) for the development roadmap from v0.1 through v2.0.

See [docs/source/development/code_quality.rst](docs/source/development/code_quality.rst) for coding standards.

## Testing

### Test Framework
The project includes an automated test suite:

```bash
# Run all tests
make test

# Or manually run individual tests
cd tests/scripts
./test_boot.sh          # Boot sequence validation
./test_integration.sh   # Full integration tests
./test_user_mode.sh     # User mode and syscalls
```

Test suite validates:
- ✓ Memory management (PMM, kmalloc, paging, DMA)
- ✓ Memory isolation (per-process page tables, VMAs, heap safety)
- ✓ Address translation (virt↔phys)
- ✓ Memory barriers (fence instructions)
- ✓ Kernel initialization and boot sequence
- ✓ Process creation and scheduling
- ✓ User-space syscalls (brk, mmap, munmap, fork)
- ✓ Memory protection and isolation
- ✓ VirtIO block device I/O
- ✓ ext2 filesystem operations
- ✓ ELF program loading and execution

### User-Space Programs

Located in `userland/`:
- **Core utilities**: cat, ls, pwd, mkdir, rmdir, touch, rm, clear, sleep
- **System utilities**: ps, uname, uptime, whoami, tty, kill
- **Shell**: ush (interactive shell with command history)
- **Test programs**: hello, clock, signal_test, pipe_test, fork_test

Programs are compiled as RISC-V ELF64 executables and can be loaded from the ext2 filesystem.

## Platform Support

- **QEMU virt machine**: Tested and working ✓

## Requirements

- RISC-V GNU Toolchain (`riscv64-unknown-elf-gcc`)
- QEMU 10.1.2+ RISC-V System Emulator (`qemu-system-riscv64`)
  - OpenSBI 1.5.1+ with SSTC extension support
  - ACLINT timer device
- Make
- Standard Unix utilities (bash, sed, etc.)
- For building QEMU: ninja, glib-2.0, pixman, slirp

## License

See [LICENSE](LICENSE) file for details.
