# ThunderOS

A RISC-V operating system specialized for AI workloads.

## Features (Planned)
- RISC-V 64-bit architecture
- Support for RISC-V Vector Extension (RVV)
- Optimized task scheduling for AI workloads
- Memory management for large models
- Hardware accelerator support

## Project Structure
```
boot/                - Bootloader and early initialization
kernel/              - Kernel core
  arch/riscv64/      - RISC-V architecture-specific code
    drivers/         - RISC-V HAL implementations (UART, timer, etc.)
    interrupt/       - Trap/interrupt handling
    cpu/             - CPU control and management
  core/              - Portable kernel core
  drivers/           - High-level device drivers
  mm/                - Memory management
  sched/             - Task scheduler
include/             - Header files
  hal/               - Hardware Abstraction Layer interfaces
  kernel/            - Kernel subsystem headers
  arch/              - Architecture-specific headers
lib/                 - Utility libraries
build/               - Build output
```

## Building
```bash
make all
```

## Running in QEMU
```bash
make qemu
```

## Documentation

Full documentation is available in Sphinx format:

```bash
cd docs
make html
# Open docs/build/html/index.html in browser
```

Online: See `docs/build/html/index.html`

## Development Roadmap
1. ✓ Project setup
2. ✓ Bootloader and kernel entry
3. ✓ UART driver
4. ✓ Documentation (Sphinx)
5. ✓ Hardware Abstraction Layer (HAL) - UART
6. [ ] HAL - Timer and interrupts
7. [ ] Memory management (physical and virtual)
8. [ ] Process management and scheduling
9. [ ] AI accelerator support

## Architecture Support

ThunderOS uses a Hardware Abstraction Layer (HAL) to support multiple architectures:

- **RISC-V 64-bit** (Primary) - Full support with RVV optimizations
- **ARM64/AArch64** (Planned) - For mobile and embedded AI
- **x86-64** (Planned) - For cloud and development

The HAL allows portable kernel code to run on any architecture while maintaining
architecture-specific optimizations in isolated modules.
