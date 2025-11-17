# Changelog

All notable changes to ThunderOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - QEMU 10.1.2 Upgrade

### Changed
- **QEMU Requirement**: Upgraded from QEMU 6.2.0 to QEMU 10.1.2
  - Replaced OpenSBI with custom M-mode initialization
  - Boot process: QEMU `-bios none` → M-mode (entry.S/start.c) → S-mode (kernel_main)
  - ISA Extensions: Added SSTC (Supervisor-mode Timer Compare), svadu, sdtrig, zicboz, zicbom
- **M-mode Implementation**: Added custom M-mode initialization (boot/entry.S, boot/start.c)
  - Replaces OpenSBI completely
  - Kernel entry point moved from 0x80200000 to 0x80000000
  - Configures medeleg, mideleg, PMP, and SSTC extension
- **Timer Driver**: Enabled SSTC extension support
  - Direct `stimecmp` CSR writes for timer programming
  - Eliminates SBI ecalls for timer operations
  - Improved timer interrupt latency
- **Interrupt Controller**: Disabled legacy CLINT initialization
  - QEMU 10.1.2 uses ACLINT with different memory layout
  - PLIC remains compatible and functional
- **Build System**:
  - Makefile: Updated QEMU path to `/tmp/qemu-10.1.2/build/qemu-system-riscv64`
  - Makefile: Changed from `-bios default` to `-bios none`
  - Linker script: Entry point changed to `_entry` (M-mode)
  - Boot sources: Now includes both .S and .c files from boot/
- **Documentation**: Updated requirements to specify QEMU 10.1.2+

### Technical Details
- **Boot Flow**: QEMU starts at 0x80000000 (M-mode) → entry.S sets up stack → start.c configures CSRs → mret to kernel_main (S-mode)
- **SSTC Extension**: Enabled via menvcfg.STCE in M-mode, allows direct stimecmp writes
- **Delegation**: mideleg=0xffff, medeleg=0xffff (all interrupts/exceptions to S-mode)
- **PMP**: All memory accessible to S-mode (pmpaddr0=0x3fffffffffffff, pmpcfg0=0xf)

## [0.4.0] - 2025-11-11 - "Persistence"

### Overview
Fourth release of ThunderOS. Adds persistent storage capabilities with VirtIO block device driver, ext2 filesystem, Virtual Filesystem abstraction layer, and ELF program loader. This milestone enables ThunderOS to store and execute programs from disk, providing true persistence across reboots.

### Added

#### VirtIO Block Device Driver (`kernel/drivers/virtio_blk.c`)
- **Modern MMIO Interface**: Implements VirtIO 1.0+ specification
  - 64-bit queue addressing (QUEUE_DESC_LOW/HIGH, QUEUE_AVAIL_LOW/HIGH, QUEUE_USED_LOW/HIGH)
  - Modern device feature negotiation
  - Non-legacy addressing mode (``-global virtio-mmio.force-legacy=false``)
- **Descriptor Ring Management**:
  - 256-entry virtqueue with descriptor table, available ring, and used ring
  - Descriptor chaining for complex I/O operations (request header → data buffer → status)
  - Free descriptor tracking and allocation
- **Block I/O Operations**:
  - ``virtio_blk_read()``: Read sectors from device (512-byte units)
  - ``virtio_blk_write()``: Write sectors to device
  - Synchronous I/O with polling (100-600 iterations typical)
- **DMA Integration**:
  - Uses DMA allocator for physically contiguous ring buffers
  - Physical address translation for all device-accessible memory
  - Memory barriers (``memory_barrier()``) for correct descriptor ordering
- **Device Initialization**:
  - Full VirtIO device initialization protocol (ACKNOWLEDGE → DRIVER → FEATURES_OK → DRIVER_OK)
  - Device capacity detection (sectors × 512 bytes)
  - MMIO base address: ``0x10008000``
- **QEMU Integration**:
  - Tested with ``-drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0``
  - Compatible with raw disk images created by ``mkfs.ext2``

#### ext2 Filesystem (`kernel/fs/ext2.c`, `kernel/fs/ext2_vfs.c`)
- **On-Disk Structure Support**:
  - Superblock parsing (magic number validation, block size calculation)
  - Block group descriptor table reading
  - Inode table access with 128 or 256-byte inodes
  - Block and inode bitmap management
- **File Operations**:
  - ``ext2_read_file()``: Read file data with offset and size
  - ``ext2_write_file()``: Write file data (allocates blocks as needed)
  - ``ext2_read_inode()``: Read inode metadata by number
  - ``ext2_write_inode()``: Update inode on disk
- **Directory Operations**:
  - ``ext2_readdir()``: Iterate through directory entries
  - ``ext2_lookup_entry()``: Find file by name in directory
  - ``ext2_path_to_inode()``: Resolve full path to inode number
  - Support for ``.`` and ``..`` entries
- **Block Addressing**:
  - Direct blocks (``i_block[0..11]``): 12 × 4KB = 48 KB
  - Single indirect blocks (``i_block[12]``): 1024 × 4KB = 4 MB
  - Files up to ~4 MB supported (double/triple indirect not yet implemented)
- **Block Allocation**:
  - ``ext2_alloc_block()``: Allocate free block from bitmap
  - ``ext2_free_block()``: Mark block as free
  - Bitmap-based allocation with superblock free count updates
- **Mounting**:
  - ``ext2_mount()``: Validate and mount ext2 filesystem from block device
  - Superblock located at offset 1024 bytes (sector 2)
  - Validates magic number (``0xEF53``), calculates block size from ``s_log_block_size``
  - Caches critical metadata (inode table location, block group descriptors)
- **Compatibility**:
  - Compatible with standard ext2 created by ``mkfs.ext2 -b 4096``
  - Supports 1KB, 2KB, and 4KB block sizes
  - Works with revision 0 and 1 ext2 filesystems

#### Virtual Filesystem (VFS) Layer (`kernel/fs/vfs.c`)
- **Mount Point Management**:
  - ``vfs_mount()``: Associate filesystem with path (e.g., ``/`` → ext2)
  - Linked list of VFS nodes for multiple mount points
  - Path resolution with longest-prefix matching
- **File Descriptor Table**:
  - Global file descriptor table (64 entries, FDs 0-63)
  - FDs 0-2 reserved for stdin/stdout/stderr
  - Tracks open files, current offsets, and flags
- **File Operations**:
  - ``vfs_open()``: Open file by path, returns file descriptor
  - ``vfs_read()``: Read from file descriptor
  - ``vfs_write()``: Write to file descriptor
  - ``vfs_seek()``: Change file offset (SEEK_SET, SEEK_CUR, SEEK_END)
  - ``vfs_close()``: Close file descriptor
- **Directory Operations**:
  - ``vfs_readdir()``: List directory contents with callback
  - ``vfs_mkdir()``: Create directory
  - ``vfs_stat()``: Get file metadata (size, permissions, timestamps)
- **VFS Operations Interface**:
  - Function pointer table for filesystem-specific implementations
  - Abstraction allows multiple filesystem types
  - ext2 implements all core VFS operations
- **Path Resolution**:
  - Converts absolute paths to filesystem-relative paths
  - Handles mount point boundaries
  - Supports nested mount points (future: ``/mnt/usb``, etc.)

#### ELF64 Program Loader (`kernel/core/elf_loader.c`)
- **ELF File Parsing**:
  - Validates ELF64 header (magic number ``0x7F 'E' 'L' 'F'``)
  - Checks RISC-V architecture (``e_machine == 0xF3``)
  - Verifies executable type (``ET_EXEC``)
  - Validates entry point address
- **Program Header Loading**:
  - Iterates through ``PT_LOAD`` segments
  - Allocates physical pages for each segment
  - Maps pages into process's page table with correct permissions:
    - Code segment: ``PTE_READ | PTE_EXECUTE`` (no write)
    - Data segment: ``PTE_READ | PTE_WRITE`` (no execute)
    - User-accessible: ``PTE_USER`` flag
- **Segment Loading**:
  - Reads file data into mapped pages
  - Handles ``p_filesz`` < ``p_memsz`` (zero-fills ``.bss`` section)
  - Supports segments at any virtual address (typically ``0x10000`` for code)
- **Process Creation**:
  - ``elf_create_process()``: Load ELF and create new process
  - Allocates separate page table for memory isolation
  - Creates user stack (8 KB at ``0x7FFFE0000000``)
  - Sets entry point (PC) to ``e_entry``
  - Sets stack pointer to stack top
  - Marks process as ready for scheduling
- **Memory Layout**:
  - Code: ``0x10000`` - ``0x11000``
  - Data: ``0x12000`` - ``0x18000``
  - User Stack: ``0x7FFFE0000000`` - ``0x7FFFE0002000`` (grows down)
- **Security**:
  - Each process has isolated address space
  - NX (no-execute) protection on data and stack
  - Read-only code pages
  - User pages cannot access kernel memory

#### Interactive Shell Enhancements (`kernel/core/shell.c`)
- **File Operations Commands**:
  - ``ls [path]``: List directory contents
    - Shows all files and subdirectories
    - Handles ``.`` and ``..`` entries
    - Defaults to ``/`` if no path specified
  - ``cat <path>``: Display file contents
    - Reads entire file and prints to console
    - Requires absolute path
    - Handles files up to memory limits
- **Program Execution**:
  - Detects paths starting with ``/`` as programs to execute
  - Loads ELF file using ``elf_create_process()``
  - Adds process to scheduler
  - Waits for process completion using ``waitpid()``
  - Displays exit status
- **Code Quality Improvements**:
  - Applied clean code standards:
    - Descriptive variable names (``argument_count``, ``file_descriptor``, ``bytes_read``)
    - Meaningful function names (``handle_ls_command``, ``handle_cat_command``)
    - Consistent formatting and indentation
  - Removed all verbose debug output
  - Silent operation (only essential output)

#### Process Management Enhancements (`kernel/core/process.c`)
- **waitpid() System Call**:
  - ``waitpid(pid, &status, options)``: Wait for child process to exit
  - Blocks parent until child terminates
  - Returns exit status code
  - Handles zombie process cleanup
- **Process Exit Status**:
  - Processes set exit code via ``SYS_EXIT`` syscall
  - Parent retrieves exit code via ``waitpid()``
  - Zombie processes cleaned up after parent reads status
- **Process Hierarchy**:
  - Tracks parent-child relationships
  - Orphaned processes adopted by init (PID 1)

### Changed

#### Build System
- **Makefile**: Added VirtIO and filesystem targets
  - ``make qemu-disk``: Run with VirtIO disk attached
  - ``userland`` target: Build user-space programs
  - Automatic disk image creation for testing
- **Dockerfile**: Added ``e2fsprogs`` package for CI
  - Enables ``mkfs.ext2`` in GitHub Actions
  - Allows test scripts to create ext2 filesystems

#### Kernel Initialization
- **main.c**: Added filesystem and storage initialization
  - VirtIO block driver initialization
  - ext2 filesystem mounting at ``/``
  - VFS root filesystem registration
  - Shell now starts with filesystem access

#### Test Scripts
- **test_syscalls.sh, test_user_mode.sh, test_user_quick.sh**:
  - Create ext2 disk images before running tests
  - Attach VirtIO block device to QEMU
  - Updated test criteria to verify VirtIO/ext2 initialization
  - Build userland programs and copy to disk image
  - Tests now validate filesystem operations

#### Documentation
- **README.md**: Updated for v0.4.0
  - Current status reflects persistence features
  - Added filesystem usage instructions
  - Updated project structure
  - Added disk image creation steps
- **Sphinx Documentation**: New comprehensive internals docs
  - ``docs/source/internals/virtio_block.rst``: Complete VirtIO driver documentation
  - ``docs/source/internals/ext2_filesystem.rst``: Full ext2 implementation guide
  - ``docs/source/internals/vfs.rst``: VFS architecture and API reference
  - ``docs/source/internals/elf_loader.rst``: ELF64 loading process documentation

### Technical Specifications

#### VirtIO Block Driver
- **MMIO Base**: ``0x10008000``
- **Queue Size**: 256 descriptors
- **I/O Mode**: Synchronous polling (100-600 iterations per operation)
- **Sector Size**: 512 bytes
- **Addressing**: Modern 64-bit physical addresses
- **DMA**: Uses ``dma_alloc()`` for ring buffers
- **Memory Barriers**: RISC-V ``fence`` instructions for ordering

#### ext2 Filesystem
- **Block Size**: 4096 bytes (configurable: 1024, 2048, 4096)
- **Inode Size**: 128 or 256 bytes
- **Max File Size**: ~4 MB (direct + single indirect blocks)
- **Max Files**: Limited by inode count (typically 2560 for 10MB disk)
- **Superblock Location**: Offset 1024 bytes (sector 2)
- **Magic Number**: ``0xEF53``

#### ELF Loader
- **Format**: ELF64 RISC-V executables
- **Linking**: Statically linked only (no dynamic linking)
- **Entry Point**: Typically ``0x10000``
- **User Stack**: 8 KB at ``0x7FFFE0000000`` (grows down)
- **Page Protection**: NX on data/stack, read-only code
- **Address Space**: Isolated per-process page tables

#### VFS Layer
- **File Descriptors**: 64 total (0-2 reserved, 3-63 for files)
- **Mount Points**: Unlimited (linked list)
- **Path Resolution**: Longest-prefix matching
- **Open Flags**: ``O_RDONLY``, ``O_WRONLY``, ``O_RDWR``, ``O_CREAT``, ``O_TRUNC``, ``O_APPEND``

### Platform Support
- **QEMU**: virt machine (tested with 128MB, 256MB RAM)
- **Disk Format**: Raw disk images with ext2 filesystem
- **Toolchain**: riscv64-unknown-elf-gcc (bare-metal)

### Dependencies
- **Toolchain**: riscv64-unknown-elf-gcc (GCC for RISC-V bare-metal)
- **Emulator**: QEMU 5.0+ with RISC-V support
- **Firmware**: OpenSBI (provided by QEMU)
- **Filesystem Tools**: e2fsprogs (``mkfs.ext2``, ``debugfs``, ``e2fsck``)
- **Documentation**: Sphinx 4.0+ (optional)

### Known Limitations

#### VirtIO Block Driver
- **Polling-Based I/O**: No interrupt support (busy-wait for completion)
- **Single Request**: Processes one I/O at a time (no batching)
- **No Error Recovery**: Limited error handling for I/O failures

#### ext2 Filesystem
- **Single Block Group**: Only supports filesystems with one block group
- **Limited File Size**: ~4 MB maximum (no double/triple indirect blocks)
- **No Journaling**: Not ext3/ext4 (no transaction support)
- **No Extended Attributes**: No xattr support
- **Synchronous Operations**: Every write waits for disk (no caching)
- **No Block Preallocation**: Allocates blocks one at a time

#### ELF Loader
- **No Dynamic Linking**: Only statically linked executables
- **Fixed Addresses**: No ASLR (Address Space Layout Randomization)
- **No Relocations**: Must be linked at fixed virtual addresses
- **Small Stack**: 8 KB user stack (no automatic growth)
- **No Arguments**: Cannot pass argc/argv/envp to programs

#### VFS Layer
- **Global File Descriptors**: All processes share FD table
- **No Special Files**: No pipes, sockets, or device files
- **No Symbolic Links**: No symlink support
- **Limited Error Handling**: Basic error codes only

### Performance Characteristics
- **VirtIO Read**: 100-600 polling iterations (microseconds in QEMU)
- **ext2 Read**: ~1-2 ms for 4KB block (includes VirtIO overhead)
- **ELF Loading**: ~10-50 ms depending on program size
- **Filesystem Mount**: ~5-10 ms (reads superblock and block group descriptors)

### Security Considerations
- **Memory Isolation**: Each process has separate page table
- **NX Protection**: Stack and data pages are non-executable
- **Code Protection**: Code pages are read-only (no self-modifying code)
- **User/Kernel Separation**: User processes cannot access kernel memory
- **No ASLR**: Programs always load at same addresses (predictable)
- **No Stack Canaries**: No buffer overflow detection
- **No DMA Protection**: No IOMMU (devices can access all physical memory)

### Breaking Changes
- **Boot Process**: Now requires VirtIO disk with ext2 filesystem
- **Test Scripts**: Updated to create disk images automatically
- **QEMU Invocation**: Requires ``-drive`` and ``-device virtio-blk-device`` flags

### Deprecated
None

### Removed
- **Verbose Debug Output**: Removed from VirtIO driver, ext2, ELF loader, and shell
  - ``"[VirtIO] Found block device"`` messages
  - ``"Loading ELF segment..."`` messages
  - ``"Mounting ext2 filesystem..."`` verbose output
  - All 14 ELF loader debug messages

### Fixed
- **GitHub Actions CI**: Added ``e2fsprogs`` to Docker for ``mkfs.ext2`` support
- **Test Scripts**: Now create VirtIO disks so tests pass in CI
- **Test Criteria**: Updated to match actual boot output (no longer looking for removed debug messages)
- **Memory Leaks**: Proper cleanup of file descriptors and page tables

---

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
