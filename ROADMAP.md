# ThunderOS Development Roadmap

This document outlines the planned development milestones for ThunderOS, a RISC-V operating system focused on AI acceleration and educational use.

## Version 0.1.0 - "First Boot" ✅ RELEASED

**Status:** Released on November 1, 2025

### Completed Features
- ✅ Bootloader and initialization
- ✅ UART driver for console output
- ✅ Interrupt handling (PLIC, CLINT)
- ✅ Timer interrupts (100ms interval)
- ✅ Physical memory manager (PMM) with bitmap allocator
- ✅ Kernel heap allocator (kmalloc) with multi-page support
- ✅ Virtual memory with Sv39 paging (identity mapping)
- ✅ Process management with PCB structures
- ✅ Context switching (RISC-V assembly)
- ✅ Round-robin scheduler with time slicing
- ✅ Preemptive multitasking (3 concurrent processes working)
- ✅ Panic handler for kernel errors with register dump
- ✅ Page table cleanup (no memory leaks)
- ✅ CHANGELOG.md created
- ✅ Comprehensive documentation (Sphinx)
- ✅ README.md updated

### Testing Completed
- ✅ QEMU virt machine (128MB, 256MB, 512MB RAM)
- ✅ All processes run concurrently without crashes
- ✅ NULL pointer checks verified
- ✅ No critical memory leaks

**Release Criteria:**
- ✅ Boots reliably on QEMU virt machine
- ✅ Multiple processes run concurrently without crashes
- ✅ No critical memory leaks
- ✅ Basic documentation complete

---

## Version 0.2.0 - "User Space" ✅ RELEASED

**Status:** Released on November 9, 2025

**Focus:** Separation of kernel and user mode

### Completed Features
- ✅ User-mode process support (U-mode)
- ✅ System call interface (13 syscalls implemented)
- ✅ Separate page tables per process
- ✅ Privilege level switching (S-mode ↔ U-mode)
- ✅ Memory isolation between processes
- ✅ Basic user-space programs (hello world, exception test)
- ✅ Exception handling for user programs

### Testing Completed
- ✅ User programs run in unprivileged mode
- ✅ System calls work reliably (13/13 implemented)
- ✅ Memory protection enforced (page faults handled gracefully)
- ✅ User process exceptions handled without system halt
- ✅ Automated test suite passes (6/6 tests)
- ✅ All processes run concurrently without crashes

**Release Criteria:**
- ✅ User programs run in unprivileged mode
- ✅ System calls work reliably
- ✅ Memory protection enforced
- ✅ At least 3 working user-space programs

---

## Version 0.3.0 - "Memory Foundation" ✅ RELEASED

**Status:** Released on November 10, 2025

**Focus:** Advanced memory management for device I/O

### Completed Features
- ✅ DMA-capable physical memory allocator
  - ✅ Allocate physically contiguous regions
  - ✅ Track physical vs virtual addresses
  - ✅ Support arbitrary-sized allocations (not just pages)
  - ✅ Zeroed memory allocation
  - ✅ Region tracking and statistics
- ✅ Virtual-to-physical address translation
  - ✅ Page table walking for kernel space
  - ✅ Reliable virt-to-phys and phys-to-virt conversion
  - ✅ Error handling for invalid addresses
  - ✅ Kernel virtual address helpers
- ✅ Memory barriers and cache control
  - ✅ RISC-V fence instructions (fence, fence.i)
  - ✅ Device I/O memory barriers
  - ✅ Read/write/data barriers
  - ✅ Compiler barriers
- ✅ Enhanced paging support
  - ✅ Better separation of physical/virtual addressing
  - ✅ DMA-safe memory regions
  - ✅ TLB flush helpers

### Testing Completed
- ✅ DMA allocator tested (allocation, deallocation, zeroing)
- ✅ Address translation verified (virt↔phys)
- ✅ Memory barriers validated
- ✅ All tests pass in QEMU

**Release Criteria:**
- ✅ DMA allocator works reliably
- ✅ Virtual-to-physical translation accurate
- ✅ Memory barriers implemented for device I/O
- ✅ Foundation ready for device drivers (VirtIO)

**Rationale:**
Initial attempt at VirtIO block driver revealed fundamental gaps in memory infrastructure. Device drivers require DMA-capable memory allocation, reliable address translation, and proper memory barriers. Building this foundation first will make device driver implementation much simpler and more robust.

---

## Version 0.4.0 - "Persistence" ✅ RELEASED

**Status:** Released on November 11, 2025

**Focus:** Filesystem and storage

### Completed Features
- ✅ VirtIO block device driver
  - ✅ Modern VirtIO (v2) with 64-bit queue addressing
  - ✅ DMA allocator integration for descriptor rings
  - ✅ Synchronous I/O with polling
  - ✅ 512-byte sector reads and writes
- ✅ ext2 filesystem implementation
  - ✅ Superblock and block group descriptor parsing
  - ✅ Inode table access and management
  - ✅ File read/write operations
  - ✅ Directory operations (readdir, lookup)
  - ✅ Block allocation and bitmap management
  - ✅ Path resolution (``/path/to/file``)
- ✅ Virtual Filesystem (VFS) abstraction layer
  - ✅ Mount point management
  - ✅ File descriptor table (open, read, write, close, seek)
  - ✅ VFS operations interface
  - ✅ ext2 integration
- ✅ ELF64 program loader
  - ✅ ELF header validation (magic, architecture, type)
  - ✅ Program header parsing (``PT_LOAD`` segments)
  - ✅ Process creation with isolated page tables
  - ✅ User stack allocation (8 KB)
  - ✅ Memory permission enforcement (NX, read-only code)
- ✅ Interactive shell enhancements
  - ✅ ``ls`` command - list directory contents
  - ✅ ``cat`` command - display file contents
  - ✅ Program execution from disk (``/bin/program``)
  - ✅ ``waitpid()`` for process completion
- ✅ Persistent program storage
  - ✅ Programs loaded from ext2 filesystem
  - ✅ Userland programs: hello, cat, ls
- ✅ Clean code standards applied throughout

### Testing Completed
- ✅ VirtIO driver tested with QEMU (100-600 polling iterations per I/O)
- ✅ ext2 filesystem read/write operations verified
- ✅ ELF programs execute correctly from disk
- ✅ Shell commands (ls, cat, program execution) working
- ✅ GitHub Actions CI passing with disk image creation
- ✅ All automated tests passing (test_syscalls.sh, test_user_mode.sh, test_user_quick.sh)

**Release Criteria:**
- ✅ VirtIO block driver works reliably
- ✅ Can read/write files reliably
- ✅ Programs can be loaded from disk
- ✅ Filesystem survives reboots
- ✅ Basic file utilities operational

---

## Version 0.5.0 - "Communication" 🚧 IN PROGRESS

**Status:** In Development (started November 20, 2025)

**Focus:** Inter-process communication and process signaling

### Planned Features

#### Signals (Foundation - Phase 1)
- [ ] Signal infrastructure
  - [ ] Signal mask per process
  - [ ] Signal pending queue
  - [ ] Signal handler registration (user-space function pointers)
  - [ ] Signal delivery during context switch
- [ ] Core signals implementation
  - [ ] SIGKILL - Terminate process (cannot be caught)
  - [ ] SIGTERM - Graceful termination request (can be handled)
  - [ ] SIGCHLD - Child process state change notification
  - [ ] SIGSTOP - Stop/pause process (cannot be caught)
  - [ ] SIGCONT - Continue stopped process
- [ ] System calls
  - [ ] `sys_kill(pid, signal)` - Send signal to process
  - [ ] `sys_signal(signum, handler)` - Register signal handler
  - [ ] `sys_sigaction(signum, act, oldact)` - Advanced signal handling
  - [ ] `sys_sigreturn()` - Return from signal handler
- [ ] Process integration
  - [ ] Signal delivery on timer interrupt
  - [ ] Signal handling in trap handler
  - [ ] Zombie process cleanup via SIGCHLD
  - [ ] Graceful process termination
- [ ] Shell commands
  - [ ] `kill` command - Send signals to processes
  - [ ] `ps` command - Show process status

#### Pipes (Phase 2)
- [ ] Pipe infrastructure
- [ ] Pipe creation and management
- [ ] Read/write operations
- [ ] SIGPIPE signal integration

#### Networking (Phase 3)
- [ ] VirtIO network driver
- [ ] Basic TCP/IP stack (port lwIP or custom)
- [ ] Socket API (socket, bind, listen, connect, send, recv)
- [ ] Simple network utilities (ping, wget)

#### Shared Memory (Optional - Phase 4)
- [ ] Shared memory support
- [ ] IPC optimization

**Release Criteria:**
- Processes can send and receive signals
- SIGKILL terminates processes reliably
- SIGCHLD notifies parent of child termination
- Signal handlers execute in user space
- Processes can communicate via pipes
- Basic networking works (ping, simple HTTP)
- All signal and IPC tests pass

**Rationale:**
Signals are the foundation for all IPC mechanisms. They enable graceful process termination (SIGTERM vs SIGKILL), parent notification when child exits (SIGCHLD for proper waitpid), and job control (SIGSTOP/SIGCONT). Pipes require SIGPIPE for broken pipe handling. Building signals first unblocks all other communication features.

---

## Version 0.6.0 - "Visual"

**Focus:** Graphics and user interface

### Planned Features
- [ ] Framebuffer console driver
- [ ] VirtIO GPU driver
- [ ] Bitmap font rendering (8x16 characters)
- [ ] Basic graphics primitives (lines, rectangles, text)
- [ ] Interactive shell with command history
- [ ] Virtual terminals (Alt+F1, Alt+F2, etc.)
- [ ] Console multiplexing

**Release Criteria:**
- Graphical console works
- Interactive shell operational
- Can switch between multiple terminals
- Works on real hardware with HDMI output

---

## Version 0.7.0 - "POSIX Lite"

**Focus:** POSIX compatibility basics

### Planned Features
- [ ] Fork and exec implementation
- [ ] Wait/waitpid for process management
- [ ] Process groups and sessions
- [ ] Job control (background/foreground processes)
- [ ] Environment variables
- [ ] Expanded syscall set (50+ syscalls)
- [ ] Basic POSIX signals implementation
- [ ] Simple shell scripting support

**Release Criteria:**
- Can run simple POSIX programs
- Basic shell scripts execute
- Process tree management works
- Job control functional

---

## Version 0.8.0 - "Tools"

**Focus:** Development environment

### Planned Features
- [ ] Full ELF loader implementation
- [ ] Dynamic linking support
- [ ] Simple text editor (nano-like)
- [ ] Embedded compiler/interpreter (TinyCC or Lua)
- [ ] GDB stub for debugging
- [ ] Developer utilities (make, grep, sed)

**Release Criteria:**
- Can compile and run programs on ThunderOS
- Self-hosting capability (compile kernel on itself)
- Developer tools available and working
- Debugging support functional

---

## Version 0.9.0 - "Performance"

**Focus:** Optimization and stability

### Planned Features
- [ ] Advanced scheduling (CFS-like algorithm)
- [ ] Slab allocator for kernel memory
- [ ] Buffer cache for disk I/O
- [ ] Profiling and performance tools
- [ ] Multi-core support (SMP)
- [ ] Load balancing across CPUs
- [ ] Performance benchmarks

**Release Criteria:**
- Significant performance improvements over v0.8
- Stable under stress tests
- Can utilize multiple CPU cores
- Benchmarks show competitive performance

---

## Version 0.10.0 - "Hardware Ready"

**Focus:** Real hardware support

### Planned Features
- [ ] Device tree parsing and configuration
- [ ] Support for multiple RISC-V boards (HiFive, BeagleV, etc.)
- [ ] USB support (if applicable to hardware)
- [ ] Power management (suspend, resume, shutdown)
- [ ] Hardware detection and auto-configuration
- [ ] Board-specific drivers (GPIO, I2C, SPI)

**Release Criteria:**
- Boots on at least 2 different RISC-V boards
- Hardware drivers are modular and maintainable
- Works reliably on physical hardware
- Power management functional

---

## Version 1.0.0 - "Foundation Complete"

**Focus:** First stable, production-ready release

### Features to Finalize
- [ ] Comprehensive automated test suite
- [ ] Complete documentation (user and developer)
- [ ] Security hardening and audit
- [ ] Bug fixes from beta testing period
- [ ] Performance benchmarks and optimization
- [ ] Example applications and demos
- [ ] Installation and deployment guides

**Release Criteria:**
- No known critical bugs
- All core features stable and documented
- Passes comprehensive test suite
- Successfully runs on real hardware
- Ready for external contributors
- Security best practices implemented

---

## Beyond 1.0 - Future Directions

### Version 1.x Series - "Ecosystem"
- Package manager and software repository
- Advanced GPU drivers and acceleration
- Sound/audio subsystem
- Container/virtualization support
- Advanced networking (IPv6, wireless)
- Desktop environment (X11 or Wayland port)

### Version 2.0 - "AI Integration" (Original Vision!)
- AI accelerator hardware support
- Machine learning framework integration (TensorFlow Lite, ONNX)
- Neural network inference engine
- Specialized AI process scheduling
- GPU compute support for ML workloads
- AI-optimized memory management

---

## Development Philosophy

### Principles
- **Quality over speed** - Each release should be stable and well-tested
- **Documentation first** - Every feature should be documented
- **Test-driven** - Automated tests for critical functionality
- **Hardware compatibility** - Work on both QEMU and real boards
- **Educational focus** - Code should be readable and well-commented

### Release Cadence
- **Minor releases (0.x):** Released when features are complete and stable
- **Patch releases (0.x.y):** As needed for bug fixes
- **Major release (1.0):** When foundation is complete and stable

### Contribution Guidelines
See `CONTRIBUTING.md` for details on how to contribute to ThunderOS development.

---


---

## Development Philosophy

---

## How to Help

Interested in contributing? Here's where we need help:

### For v0.3 (Current)
- DMA allocator implementation
- Address translation (virt-to-phys)
- Memory barriers for device I/O
- Enhanced paging support

### For v0.4+ (Future)
- Driver development (storage, network, graphics)
- User-space utilities and programs
- Documentation and tutorials
- Testing on real RISC-V hardware

See the [Issues](https://github.com/cmelnu/thunderos/issues) page for specific tasks.

---

**Last Updated:** November 2025

For detailed technical documentation, see [docs/](docs/) directory.
