# Changelog

All notable changes to ThunderOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2025-11-10 - "Memory Foundation"

### Overview
Third release of ThunderOS. Adds advanced memory management infrastructure required for device drivers, including DMA allocation, address translation, and memory barriers. This release builds the foundation for VirtIO device drivers in v0.4.0.

### Added

#### DMA Memory Allocator (`kernel/mm/dma.c`, `include/mm/dma.h`)
- **Physically Contiguous Allocation**: Allocate multi-page regions with guaranteed physical contiguity
  - `dma_alloc()` - Allocate DMA-capable memory regions
  - `dma_free()` - Free DMA regions
  - Supports arbitrary sizes (not just page-aligned)
- **Address Tracking**: Track both physical and virtual addresses for each region
  - `dma_region_t` structure with physical/virtual base addresses and size
  - Linked list tracking of all allocated regions
- **Memory Zeroing**: Optional `DMA_ZERO` flag for zeroed allocation
- **Statistics**: Track total/free/used DMA memory via `dma_get_stats()`
- **Region Validation**: Magic number validation for corruption detection

#### Memory Barriers (`include/arch/barrier.h`)
- **RISC-V Fence Instructions**: Complete memory barrier implementation
  - `memory_barrier()` - Full memory barrier (fence rw, rw)
  - `write_barrier()` - Write memory barrier (fence w, w)
  - `read_barrier()` - Read memory barrier (fence r, r)
  - `io_barrier()` - I/O memory barrier for device access
  - `data_memory_barrier()` - Data memory barrier (DMB equivalent)
  - `data_sync_barrier()` - Data synchronization barrier (DSB equivalent)
  - `instruction_sync_barrier()` - Instruction synchronization (fence.i)
  - `compiler_barrier()` - Compiler optimization barrier
- **Documentation**: Inline documentation for each barrier type and use case

#### Enhanced Address Translation (`kernel/mm/paging.c`, `include/mm/paging.h`)
- **Virtual to Physical Translation**:
  - `translate_virt_to_phys()` - Walk page tables to get physical address
  - Error handling for invalid/unmapped addresses
  - Support for kernel virtual addresses
- **Physical to Virtual Translation**:
  - `translate_phys_to_virt()` - Reverse translation (identity map assumption)
  - `is_kernel_virt_addr()` - Check if address is in kernel virtual range
  - `phys_to_kernel_virt()` - Convert physical to kernel virtual
  - `kernel_virt_to_phys()` - Convert kernel virtual to physical
- **Page Table Walking**: Manual page table traversal for address resolution
- **TLB Management**: Helper functions for TLB flushing

#### Testing Framework (`tests/test_memory_mgmt.c`)
- **Comprehensive Test Suite**: 10 tests for new memory features
  - DMA allocator tests (allocation, deallocation, zeroing, statistics)
  - Address translation tests (virt↔phys, kernel helpers)
  - Memory barrier validation
  - All tests pass in QEMU

#### Documentation
- **Sphinx Documentation**:
  - `docs/source/internals/dma.rst` - DMA allocator internals
  - `docs/source/internals/barrier.rst` - Memory barrier guide
  - `docs/source/internals/paging.rst` - Enhanced paging documentation
  - API reference for all new functions
  - Usage examples and best practices

### Changed

#### Build System
- **Makefile**: Updated for `riscv64-unknown-elf-` toolchain
  - Changed from `riscv64-linux-gnu-` to bare-metal toolchain
  - Added test suite integration
  - Memory management tests now run automatically
- **Dockerfile**: Updated for CI/CD compatibility
  - Installs `riscv64-unknown-elf-gcc` from official releases
  - Includes QEMU for testing
  - Matches GitHub Actions environment

#### Kernel Initialization
- **main.c**: Added DMA allocator initialization
  - `dma_init()` called during boot sequence
  - Memory management tests invoked after init

#### Documentation
- **README.md**: Updated for v0.3.0
  - Current status reflects memory foundation features
  - Test suite description updated
  - Project structure updated with new components

### Technical Specifications

#### DMA Allocator
- **Region Size**: Arbitrary (rounded up to page boundaries internally)
- **Alignment**: Page-aligned (4KB)
- **Tracking**: Linked list of `dma_region_t` structures
- **Magic Number**: 0xDEADBEEF for corruption detection
- **Statistics**: Total/free/used memory tracking

#### Memory Barriers
- **Architecture**: RISC-V fence instructions
- **Ordering**: Enforces memory operation ordering for device I/O
- **Granularity**: Per-operation barriers (read, write, full, I/O)

#### Address Translation
- **Page Table Format**: RISC-V Sv39 (39-bit virtual addressing)
- **Translation Levels**: 3-level page table walk
- **Error Handling**: Returns 0 for invalid/unmapped addresses
- **Identity Mapping**: Kernel uses identity map (virt == phys for most kernel memory)

### Platform Support
- **QEMU**: virt machine (tested and working with 128MB, 256MB, 512MB RAM)
- **Toolchain**: riscv64-unknown-elf-gcc (bare-metal)

### Dependencies
- **Toolchain**: riscv64-unknown-elf-gcc (GCC for RISC-V bare-metal)
- **Emulator**: QEMU 5.0+ with RISC-V support
- **Firmware**: OpenSBI (provided by QEMU)
- **Documentation**: Sphinx 4.0+ (optional)

### Known Limitations

#### DMA Allocator
- **No Fragmentation Handling**: Allocations may fail if memory is fragmented
- **Linear Search**: Region tracking uses linked list (O(n) lookups)
- **No IOMMU Support**: Direct physical memory access (no DMA remapping)

#### Address Translation
- **Identity Map Assumption**: `translate_phys_to_virt()` assumes identity mapping
- **Kernel Space Only**: Translation primarily designed for kernel addresses
- **No TLB Optimization**: Every translation walks page tables (not cached)

### Performance Characteristics
- **DMA Allocation**: O(n) where n = number of pages (bitmap scan)
- **Address Translation**: O(1) for 3-level page table walk
- **Memory Barriers**: 1-2 cycles per fence instruction

### Security Considerations
- **DMA Safety**: DMA regions are physically contiguous (required for device DMA)
- **No IOMMU**: Devices can access all physical memory
- **Memory Barriers**: Prevent speculative execution vulnerabilities around device I/O

### Breaking Changes
None (backward compatible with v0.2.0)

### Deprecated
None

### Removed
None

### Fixed
- **CI Build**: Fixed GitHub Actions build failure with correct toolchain
- **Compiler Warnings**: Resolved warnings in paging and DMA code

---

## [0.2.0] - 2025-11-09 - "User Space"

### Overview
Second release of ThunderOS. Adds user-mode process support with privilege separation, system call interface, and memory isolation between processes.

### Added

#### User Mode Support
- **U-mode Processes**: Processes can run in unprivileged user mode
  - Privilege level switching (S-mode ↔ U-mode)
  - User-mode exception handling
  - User stack allocation and management

#### System Call Interface
- **13 System Calls Implemented**:
  - `SYS_WRITE` - Write to console
  - `SYS_EXIT` - Terminate process
  - `SYS_GETPID` - Get process ID
  - `SYS_YIELD` - Yield CPU voluntarily
  - `SYS_GETTIME` - Get system time
  - And 8 more syscalls
- **Trap Handler**: User-mode trap handling with syscall dispatch
- **Parameter Passing**: Arguments passed via registers (a0-a5)

#### Memory Isolation
- **Separate Page Tables**: Each process has its own page table
- **Memory Protection**: Page faults for invalid memory access
- **Kernel/User Separation**: User processes cannot access kernel memory directly

#### User Programs
- **user_hello.c**: Demonstrates syscalls (write, getpid, gettime, yield, exit)
- **Exception Test**: Validates user exception handling
- **Test Suite**: Automated tests for user-mode functionality

#### Testing
- **test_syscalls.sh**: Comprehensive syscall testing script
- **test_user_quick.sh**: Quick validation of user-mode features
- **6/6 Tests Passing**: All automated tests pass

### Changed
- **Process Management**: Enhanced to support user/kernel mode switching
- **Page Tables**: Each process now has separate page table
- **Trap Handler**: Extended to handle user-mode exceptions and syscalls

### Fixed
- User process exceptions now handled gracefully without kernel panic
- Memory protection enforced between processes

---

## [0.1.0] - 2025-11-01 - "First Boot"

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
