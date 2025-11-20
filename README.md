# ThunderOS

A RISC-V operating system focused on AI acceleration and educational use.

## Current Status

**Version 0.4.0 - "Persistence"** 🎯 Released!

- ✅ **v0.4.0 Released** - Persistent storage with VirtIO and ext2
- ✅ VirtIO block device driver (modern MMIO interface)
- ✅ ext2 filesystem with read/write support
- ✅ Virtual Filesystem (VFS) abstraction layer
- ✅ ELF64 loader for executing programs from disk
- ✅ Interactive shell with ls, cat, and program execution
- ✅ **Memory isolation** - Per-process page tables, VMAs, isolated heaps
- 🚧 **Next**: Inter-process communication and networking (v0.5.0)

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

### Running with Filesystem
```bash
# Create a disk image with ext2 filesystem
./build_disk.sh

# Run with VirtIO block device
make qemu-disk
```

The OS will mount the ext2 filesystem and you can interact with files using shell commands.

### Automated Testing
```bash
# Run comprehensive syscall tests
tests/test_syscalls.sh

# Run user-mode tests
tests/test_user_mode.sh

# Run quick validation
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
# Run memory management tests
make test

# Run syscall tests
./tests/test_syscalls.sh

# Run quick validation
./tests/test_user_quick.sh
```

Test suite validates:
- ✓ DMA allocator (contiguous allocation, zeroing)
- ✓ Address translation (virt↔phys)
- ✓ Memory barriers (fence instructions)
- ✓ Kernel initialization
- ✓ Process creation and scheduling
- ✓ User-space syscalls
- ✓ Memory protection
- ✓ VirtIO block device I/O
- ✓ ext2 filesystem operations
- ✓ ELF program loading and execution

### User-Space Programs

Located in `userland/`:
- **hello.c** - Simple hello world program
- **cat.c** - Display file contents
- **ls.c** - List directory contents

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
