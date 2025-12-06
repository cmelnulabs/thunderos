# Memory Debugging Guide for ThunderOS

This guide covers memory debugging and profiling for ThunderOS using existing tools - **no code changes required**.

## Quick Start

```bash
# Enable QEMU memory tracing
make qemu QEMU_EXTRA="-d guest_errors,unimp -D qemu.log"

# Or use GDB for interactive debugging
make debug
```

## Table of Contents

1. [QEMU Built-in Tracing](#qemu-built-in-tracing)
2. [GDB Memory Debugging](#gdb-memory-debugging)
3. [QEMU Monitor Commands](#qemu-monitor-commands)
4. [Common Debug Scenarios](#common-debug-scenarios)

---

## QEMU Built-in Tracing

QEMU has powerful built-in tracing and logging capabilities that require zero code changes.

### Basic Memory Logging

Enable guest error and unimplemented feature logging:

```bash
qemu-system-riscv64 \
    -machine virt -m 128M \
    -nographic -serial mon:stdio \
    -bios none \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/fs.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -d guest_errors,unimp \
    -D qemu.log
```

**Flags:**
- `-d guest_errors` - Log guest errors (page faults, invalid memory access)
- `-d unimp` - Log unimplemented features accessed by guest
- `-D qemu.log` - Write logs to file instead of stderr

### All Available Debug Options

List all QEMU debug options:
```bash
qemu-system-riscv64 -d help
```

**Useful options for memory debugging:**

| Flag | Description |
|------|-------------|
| `-d in_asm` | Show generated assembly (TCG) |
| `-d out_asm` | Show host assembly output |
| `-d int` | Log interrupts and exceptions |
| `-d mmu` | Log MMU-related activity (page tables, TLB) |
| `-d page` | Log page allocations |
| `-d guest_errors` | Log guest OS errors |
| `-d unimp` | Log unimplemented device/feature access |
| `-d cpu` | Log CPU state changes |
| `-d exec` | Show instruction execution trace |

### Memory Access Tracing

For detailed memory access tracing, use QEMU trace events:

```bash
# List available trace events
qemu-system-riscv64 -trace help

# Enable memory-related traces
qemu-system-riscv64 \
    ... (kernel args) ... \
    -trace 'memory_region_*' \
    -trace 'load_*' \
    -trace 'store_*' \
    -D memory_trace.log
```

**Common trace patterns:**
- `memory_region_*` - Memory region operations
- `load_*` - Load operations from memory
- `store_*` - Store operations to memory
- `dma_*` - DMA operations

### Example: Debug Memory Leak

1. Run kernel with logging:
   ```bash
   qemu-system-riscv64 ... -d guest_errors,page -D memory.log
   ```

2. Check log for page allocations:
   ```bash
   grep "page" memory.log
   ```

3. Look for unfreed pages or growing memory usage

---

## GDB Memory Debugging

GDB provides interactive memory inspection and watchpoints.

### Start GDB Session

**Terminal 1** - Start QEMU with GDB server:
```bash
qemu-system-riscv64 \
    -machine virt -m 128M \
    -nographic -serial mon:stdio \
    -bios none \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/fs.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -s -S
```

**Flags:**
- `-s` - Start GDB server on `localhost:1234`
- `-S` - Freeze CPU at startup (wait for GDB)

**Terminal 2** - Connect GDB:
```bash
riscv64-unknown-elf-gdb build/thunderos.elf

(gdb) target remote :1234
(gdb) continue
```

Or use the Makefile target:
```bash
# Terminal 1
make debug

# Terminal 2
make gdb
```

### Memory Inspection Commands

```gdb
# Examine memory at address
(gdb) x/10x 0x80000000        # 10 hex words
(gdb) x/10i 0x80000000        # 10 instructions
(gdb) x/10s 0x80000000        # 10 strings
(gdb) x/10c 0x80000000        # 10 characters

# Watch memory location
(gdb) watch *(int*)0x80001000  # Break on write
(gdb) rwatch *(int*)0x80001000 # Break on read
(gdb) awatch *(int*)0x80001000 # Break on access

# Examine variables
(gdb) print g_alloc_count
(gdb) print/x g_process_table[0]

# Memory regions
(gdb) info mem               # Show memory regions
(gdb) info proc mappings     # Show process memory map (if supported)
```

### Find Memory Leaks with GDB

```gdb
# Set breakpoint at kmalloc
(gdb) break kmalloc
(gdb) commands
  > silent
  > printf "kmalloc(%d) = ", $a0
  > finish
  > printf "%p\n", $a0
  > continue
  > end

# Set breakpoint at kfree
(gdb) break kfree
(gdb) commands
  > silent
  > printf "kfree(%p)\n", $a0
  > continue
  > end

# Run and analyze alloc/free pairs
(gdb) continue
```

### GDB Script for Memory Tracking

Create `gdb_memory.py`:

```python
import gdb

class TrackAllocations(gdb.Command):
    def __init__(self):
        super(TrackAllocations, self).__init__("track-alloc", gdb.COMMAND_USER)
        self.allocations = {}
    
    def invoke(self, arg, from_tty):
        # Set breakpoint on kmalloc
        bp_malloc = gdb.Breakpoint("kmalloc")
        bp_malloc.silent = True
        bp_malloc.commands = "python TrackAllocations.on_malloc()"
        
        # Set breakpoint on kfree
        bp_free = gdb.Breakpoint("kfree")
        bp_free.silent = True
        bp_free.commands = "python TrackAllocations.on_free()"
        
        gdb.execute("continue")
    
    def on_malloc():
        size = gdb.parse_and_eval("$a0")
        gdb.execute("finish")
        addr = gdb.parse_and_eval("$a0")
        TrackAllocations.allocations[addr] = size
        print(f"Allocated {size} bytes at {addr}")
    
    def on_free():
        addr = gdb.parse_and_eval("$a0")
        if addr in TrackAllocations.allocations:
            del TrackAllocations.allocations[addr]
            print(f"Freed {addr}")
        else:
            print(f"WARNING: Double free or invalid free at {addr}")

TrackAllocations()
```

Load in GDB:
```gdb
(gdb) source gdb_memory.py
(gdb) track-alloc
```

---

## QEMU Monitor Commands

Access QEMU monitor while kernel is running by pressing `Ctrl-A C` (then `Ctrl-A C` again to return to console).

### Useful Monitor Commands

```
# Memory information
(qemu) info mem              # Show page table mappings
(qemu) info tlb              # Show TLB contents
(qemu) info registers        # Show CPU registers

# Memory operations
(qemu) x /10x 0x80000000     # Examine physical memory
(qemu) xp /10x 0x80000000    # Examine physical memory (explicit)

# System state
(qemu) info mtree            # Memory tree (all memory regions)
(qemu) info qtree            # Device tree

# Save/restore state
(qemu) savevm snap1          # Save VM snapshot
(qemu) loadvm snap1          # Load VM snapshot

# Tracing control
(qemu) trace-event memory_region_ops on
(qemu) trace-event memory_region_ops off
```

### Memory Tree Analysis

```
(qemu) info mtree
```

This shows all memory regions including:
- Physical RAM mapping
- MMIO device regions (UART, VirtIO, CLINT, PLIC)
- ROM regions
- Overlapping regions (potential bugs!)

---

## Common Debug Scenarios

### 1. Track Down Memory Corruption

**Symptoms:** Random crashes, data corruption

**Debug steps:**

```bash
# Run with memory and exception logging
qemu-system-riscv64 ... -d guest_errors,int,mmu -D debug.log

# Check for:
grep "page fault" debug.log
grep "invalid access" debug.log
```

**GDB approach:**

```gdb
# Watch specific memory location
(gdb) watch *(uint64_t*)0x80005000
(gdb) continue

# When hit, examine backtrace
(gdb) backtrace
(gdb) info registers
```

### 2. Find Memory Leaks

**Quick check without tools:**

```bash
# Run kernel, note memory at different points
# Add this to your kernel main.c temporarily:

void print_memory_stats(void) {
    extern uint64_t pmm_get_free_pages(void);
    kprintf("Free pages: %d\n", pmm_get_free_pages());
}

# Call at various points in execution
```

**QEMU approach:**

```bash
# Monitor page allocations
qemu-system-riscv64 ... -d page -D page_alloc.log

# Analyze:
grep "page alloc" page_alloc.log | wc -l
grep "page free" page_alloc.log | wc -l
# If alloc > free, you have a leak
```

### 3. Debug DMA Issues

**Symptoms:** VirtIO timeouts, device errors

```bash
# Enable DMA tracing
qemu-system-riscv64 ... \
    -trace 'dma_*' \
    -trace 'virtio_*' \
    -D dma_trace.log

# Check log for:
# - Invalid physical addresses
# - Misaligned addresses  
# - Out-of-bounds access
```

### 4. Page Fault Debugging

```bash
# Run with MMU logging
qemu-system-riscv64 ... -d int,mmu -D mmu.log

# Look for:
grep "exception" mmu.log
grep "page fault" mmu.log
```

**GDB approach:**

```gdb
# Break on trap handler
(gdb) break trap_handler
(gdb) continue

# When hit, check cause
(gdb) print/x $scause
# 0xc = Instruction page fault
# 0xd = Load page fault  
# 0xf = Store page fault

(gdb) print/x $stval    # Faulting address
(gdb) backtrace         # Call stack
```

### 5. Heap Corruption

**Symptoms:** kmalloc returns NULL, corrupted data structures

```gdb
# Break on kmalloc and kfree
(gdb) break kmalloc
(gdb) break kfree

# Check heap integrity
(gdb) print *((kmalloc_header*)0x80060000)

# Verify magic number
(gdb) print/x ((kmalloc_header*)0x80060000)->magic
# Should be 0xDEADBEEF
```

### 6. Stack Overflow Detection

```gdb
# Set watchpoint on stack guard area
# Assuming stack at 0x80050000, size 4KB

(gdb) watch *(uint64_t*)0x8004F000
(gdb) continue

# Or manually check
(gdb) x/10x 0x8004F000
# Look for corrupted stack canary
```

---

## Makefile Integration

Add these targets to your Makefile:

```makefile
# Debug with memory logging
.PHONY: qemu-debug-mem
qemu-debug-mem: $(BUILD_DIR)/$(KERNEL_ELF) $(BUILD_DIR)/fs.img
	@echo "Starting QEMU with memory debugging..."
	qemu-system-riscv64 \
		-machine virt -m 128M \
		-nographic -serial mon:stdio \
		-bios none \
		-kernel $(BUILD_DIR)/$(KERNEL_ELF) \
		-global virtio-mmio.force-legacy=false \
		-drive file=$(BUILD_DIR)/fs.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0 \
		-d guest_errors,int,mmu \
		-D memory_debug.log

# Debug with GDB
.PHONY: qemu-gdb
qemu-gdb: $(BUILD_DIR)/$(KERNEL_ELF) $(BUILD_DIR)/fs.img
	@echo "Starting QEMU with GDB server (port 1234)..."
	@echo "In another terminal, run: make gdb"
	qemu-system-riscv64 \
		-machine virt -m 128M \
		-nographic -serial mon:stdio \
		-bios none \
		-kernel $(BUILD_DIR)/$(KERNEL_ELF) \
		-global virtio-mmio.force-legacy=false \
		-drive file=$(BUILD_DIR)/fs.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0 \
		-s -S

.PHONY: gdb
gdb: $(BUILD_DIR)/$(KERNEL_ELF)
	riscv64-unknown-elf-gdb $(BUILD_DIR)/$(KERNEL_ELF) \
		-ex "target remote :1234" \
		-ex "layout split"
```

Usage:
```bash
# Memory debugging
make qemu-debug-mem
tail -f memory_debug.log

# GDB debugging (two terminals)
make qemu-gdb           # Terminal 1
make gdb                # Terminal 2
```

---

## Tips and Best Practices

### 1. Start Simple
Begin with basic logging (`-d guest_errors`) and add more verbose options only when needed.

### 2. Filter Logs
QEMU logs can be massive. Use grep/awk:
```bash
# Only show page faults
grep "page fault" qemu.log

# Count memory accesses to specific region
grep "0x10000000" memory_trace.log | wc -l
```

### 3. Use Snapshots
Save VM state before testing suspicious code:
```
(qemu) savevm before_test
# Run test
(qemu) loadvm before_test
```

### 4. Combine Tools
Use QEMU logging for overview, GDB for precise inspection:
```bash
# Find where problem occurs (QEMU log)
grep "invalid" qemu.log
# "Invalid access at 0x80001234"

# Investigate with GDB
(gdb) break *0x80001234
(gdb) continue
(gdb) backtrace
```

### 5. Automate Common Tasks
Create shell scripts for repetitive debugging:

```bash
#!/bin/bash
# debug_memory.sh

echo "Building kernel..."
make clean && make

echo "Starting QEMU with memory logging..."
timeout 30 qemu-system-riscv64 \
    ... (args) ... \
    -d guest_errors,mmu \
    -D memory.log

echo "Analyzing log..."
echo "Page faults: $(grep -c 'page fault' memory.log)"
echo "Invalid accesses: $(grep -c 'invalid' memory.log)"
grep "exception" memory.log | head -20
```

---

## References

- [QEMU Documentation - System Emulation](https://www.qemu.org/docs/master/system/index.html)
- [QEMU Tracing Documentation](https://www.qemu.org/docs/master/devel/tracing.html)
- [GDB Manual](https://sourceware.org/gdb/current/onlinedocs/gdb/)
- [RISC-V GDB](https://github.com/riscv/riscv-gnu-toolchain)

---

## Quick Reference Card

```
# QEMU Debug Flags
-d guest_errors       Guest OS errors
-d int                Interrupts/exceptions
-d mmu                MMU operations
-d page               Page allocations
-D logfile.txt        Log output file

# GDB Commands
x/FMT ADDR           Examine memory
watch EXPR           Break on write
info mem             Memory regions
backtrace            Call stack

# QEMU Monitor (Ctrl-A C)
info mem             Page tables
info mtree           Memory tree
x /FMT ADDR          Examine memory
```

---

**No code changes. No maintenance overhead. Just use the tools that already exist!** 🎯
