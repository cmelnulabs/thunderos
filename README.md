# ThunderOS

A RISC-V operating system focused on AI acceleration and educational use.

## Current Status

**Version 0.1.0 - "First Boot"** 🎯

See [CHANGELOG.md](CHANGELOG.md) for complete feature list and [ROADMAP.md](ROADMAP.md) for future plans.

## Quick Start

### Building
```bash
make all
```

### Running in QEMU
```bash
make qemu
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

## Platform Support

- **QEMU virt machine**: Tested and working ✓

## Dependencies

- **Toolchain**: riscv64-unknown-elf-gcc (GCC for RISC-V)
- **Emulator**: QEMU 5.0+ with RISC-V support
- **Firmware**: OpenSBI (provided by QEMU)
- **Documentation** (optional): Sphinx 4.0+

## License

See [LICENSE](LICENSE) file for details.
