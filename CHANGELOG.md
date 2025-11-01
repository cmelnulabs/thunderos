# Changelog

All notable changes to ThunderOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Before v0.1.0 Release
- Comprehensive testing on QEMU with different RAM configurations
- Memory leak verification
- Extended runtime stability testing
- Documentation review and updates

---

## [0.1.0] - TBD - "First Boot"

### Overview
First functional release of ThunderOS. Provides basic kernel infrastructure with preemptive multitasking on RISC-V 64-bit architecture running in QEMU.

### Added

#### Core Kernel
- **Bootloader**: RISC-V assembly bootloader with stack setup and BSS clearing
- **Kernel Entry**: C kernel entry point with hardware initialization sequence
- **Panic Handler**: Fatal error handling with register dump and system halt
  - `kernel_panic()` function for unrecoverable errors
  - `KASSERT()`, `BUG()`, and `NOT_IMPLEMENTED()` macros
  - Displays RISC-V CSR registers (sstatus, sepc, scause, stval)

#### Hardware Abstraction Layer (HAL)
- **UART Driver**: NS16550A-compatible serial console
  - Character output (`hal_uart_putc`)
  - String output (`hal_uart_puts`)
  - Initialization and detection
- **Timer Driver**: RISC-V timer with CLINT integration
  - Configurable interval (default: 100ms)
  - Tick counter management
  - Microsecond/millisecond delay functions

#### Interrupt Handling
- **Trap Handler**: Unified exception and interrupt handling
  - Supervisor mode trap vector
  - Exception names and cause decoding
  - Timer interrupt routing to scheduler
- **PLIC Driver**: Platform-Level Interrupt Controller support
- **CLINT Driver**: Core Local Interrupt controller for timer

#### Memory Management
- **Physical Memory Manager (PMM)**:
  - Bitmap-based page allocator
  - 4KB page granularity
  - Single and contiguous multi-page allocation
  - Supports up to 32MB RAM (expandable)
- **Kernel Heap (kmalloc)**:
  - Page-based allocator with metadata headers
  - Multi-page allocation support
  - Heap corruption detection (magic number validation)
  - Free and allocation tracking
- **Virtual Memory**:
  - RISC-V Sv39 paging (39-bit virtual addresses)
  - Identity mapping for kernel and RAM
  - MMIO device mapping (UART, CLINT)
  - Page table management
  - Address translation functions

#### Process Management
- **Process Control Blocks (PCB)**:
  - Up to 64 concurrent processes
  - Process states: UNUSED, EMBRYO, READY, RUNNING, SLEEPING, ZOMBIE
  - Per-process kernel and user stacks (8KB each)
  - Process name, PID, parent tracking
  - CPU time accounting
- **Context Switching**:
  - RISC-V assembly implementation
  - Saves/restores callee-saved registers (s0-s11, sp, ra)
  - Atomic state transitions
  - Interrupt-safe switching
- **Scheduler**:
  - Round-robin scheduling algorithm
  - Time-slicing (1-second quantum = 10 timer ticks)
  - Ready queue implementation
  - Preemptive multitasking on timer interrupts
  - Voluntary yielding via `process_yield()`

#### Utilities
- **Kernel String Functions**:
  - `kstrcpy()`, `kstrncpy()`, `kstrlen()`
  - `kmemset()`, `kmemcpy()`
  - `kprint_dec()` - decimal number printing
  - `kprint_hex()` - hexadecimal number printing with 0x prefix

#### Testing Framework
- **KUnit-inspired framework**:
  - Test case definition macros
  - Assertion macros (ASSERT_EQ, ASSERT_NE, ASSERT_TRUE, ASSERT_FALSE)
  - Test result reporting
  - Tests for paging, timer, and trap handling

#### Documentation
- **Sphinx Documentation**:
  - Architecture overview
  - Component internals (bootloader, UART, traps, interrupts, HAL, memory, processes)
  - RISC-V primer
  - Development guide
  - Code quality standards
  - API reference

#### Build System
- **Makefile**:
  - Cross-compilation for RISC-V (rv64gc)
  - Separate build directories
  - QEMU integration (`make qemu`)
  - GDB debugging support (`make debug`)
  - Disassembly generation (`make dump`)

### Technical Specifications

#### Architecture
- **Target**: RISC-V 64-bit (rv64gc)
- **ISA Extensions**: G (IMAFD), C (compressed)
- **ABI**: LP64D (64-bit long/pointer, double-precision float)
- **Code Model**: Medium Any (position-independent)
- **Privilege Levels**: Machine-mode (OpenSBI), Supervisor-mode (kernel)

#### Memory Layout
- **Kernel Load Address**: 0x80200000
- **RAM**: 128MB (default QEMU configuration)
- **Page Size**: 4KB
- **Kernel Stack**: 8KB per process
- **User Stack**: 8KB per process (kernel space for now)
- **Heap**: Dynamic via kmalloc (page-based)

#### Timing
- **Timer Frequency**: 10 MHz (QEMU default)
- **Timer Interval**: 100ms (100,000 microseconds)
- **Scheduler Time Slice**: 1 second (10 timer interrupts)

### Known Limitations

#### Memory Management
- **Identity Mapping Only**: Virtual addresses equal physical addresses
  - Kernel not in higher-half address space
  - Simplifies implementation but limits flexibility
- **Shared Page Table**: All processes use kernel page table
  - No memory isolation between processes
  - All memory is accessible to all processes
- **Page Table Memory Leak**: Page tables not freed on process termination

#### Process Management
- **Supervisor Mode Only**: All processes run in S-mode
  - No privilege separation
  - Processes have full kernel access
- **Fixed Process Limit**: Hard-coded maximum of 64 processes
  - Array-based process table, not dynamic

#### Scheduler
- **Equal Time Slices**: All processes get same 1-second quantum
  - No priority-based scheduling
  - No real-time guarantees
- **Single CPU**: No multi-core support
  - All processes run on one CPU

#### I/O
- **Serial Console Only**: UART is the only I/O device
- **Polling-Based UART**: UART uses polling, not interrupts

#### Testing and Validation
- **Limited Automated Tests**: Most testing is manual observation
  - Test framework exists but coverage is minimal
- **QEMU Only**: Not tested on physical RISC-V hardware

### Platform Support
- **QEMU**: virt machine (tested and working)

### Dependencies
- **Toolchain**: riscv64-unknown-elf-gcc (GCC for RISC-V)
- **Emulator**: QEMU 5.0+ with RISC-V support
- **Firmware**: OpenSBI v0.9 (provided by QEMU)
- **Documentation**: Sphinx 4.0+ (optional, for building docs)

### Performance Characteristics
- **Boot Time**: ~1 second in QEMU
- **Context Switch**: Microseconds (exact timing not measured)
- **Memory Overhead**: ~60KB kernel, ~16KB per process (stacks)
- **Supported Processes**: 64 maximum (configurable)

### Security Considerations
- **No Security Model**: This is an educational/experimental OS
  - No user authentication
  - No access control
  - No memory protection between processes
  - All code runs in supervisor mode
- **Not Production Ready**: Do not use for any security-sensitive applications

### Breaking Changes
None (first release)

### Deprecated
None (first release)

### Removed
None (first release)

### Fixed
Not applicable (first release)

---

## Links and References

- [ROADMAP.md](ROADMAP.md) - Development roadmap and planned features
- [Documentation](docs/build/html/index.html) - Full technical documentation
- [LICENSE](LICENSE) - License information

---

**Note**: This changelog follows the principles of [Keep a Changelog](https://keepachangelog.com/).
