# User Mode Implementation Guide

## Overview

This guide details the complete implementation of user mode support in ThunderOS, including architecture, code organization, and implementation details.

## Table of Contents

1. [Architecture](#architecture)
2. [Component Breakdown](#component-breakdown)
3. [Implementation Details](#implementation-details)
4. [Testing Strategy](#testing-strategy)
5. [Troubleshooting](#troubleshooting)

## Architecture

### High-Level Design

```
┌─────────────────────────────────────────────────────────────┐
│                    User Process                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Code (0x10000)                                  │   │
│  │ - Executable (R, X, U)                               │   │
│  │ - PTE_USER_TEXT                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Data/Heap                                       │   │
│  │ - Readable, writable (R, W, U)                       │   │
│  │ - PTE_USER_DATA                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ User Stack (0x80000000 - 1MB)                        │   │
│  │ - Readable, writable (R, W, U)                       │   │
│  │ - PTE_USER_DATA                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│           Uses own Page Table (satp)                        │
└─────────────────────────────────────────────────────────────┘
              ↓ Trap (exception/interrupt/ecall) ↓
┌─────────────────────────────────────────────────────────────┐
│                  Kernel (Supervisor Mode)                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ trap_entry.S                                         │   │
│  │ - Stack switching via sscratch                       │   │
│  │ - Save trap frame to kernel stack                    │   │
│  │ - Call trap_handler()                                │   │
│  └──────────────────────────────────────────────────────┘   │
│           ↓                                                  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ trap_handler (C code)                                │   │
│  │ - Handle exception/interrupt/syscall                 │   │
│  │ - Modify trap frame as needed                        │   │
│  │ - Return to user or kernel mode                      │   │
│  └──────────────────────────────────────────────────────┘   │
│           ↓                                                  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Trap exit                                            │   │
│  │ - Check SPP bit in sstatus                           │   │
│  │ - Restore registers and sscratch                     │   │
│  │ - Execute sret                                       │   │
│  └──────────────────────────────────────────────────────┘   │
│   Contains Kernel Code/Data + MMIO + shared mappings        │
└─────────────────────────────────────────────────────────────┘
```

### Privilege Level Model

```
┌─────────────────────────────────┐
│  Machine Mode (M)               │
│  - Firmware/Bootloader          │
│  - Full hardware access         │
└──────────────┬──────────────────┘
               │
        sret (SPP=1)
               ↓
┌─────────────────────────────────┐
│  Supervisor Mode (S)            │
│  - ThunderOS Kernel             │
│  - Protected hardware access    │
│  - Exception/interrupt handling │
│  - Page table management        │
└──────────────┬──────────────────┘
               │
        sret (SPP=0) / user_return
               ↓
┌─────────────────────────────────┐
│  User Mode (U)                  │
│  - Application code             │
│  - Limited CPU features         │
│  - Syscalls for OS services    │
└─────────────────────────────────┘
```

## Component Breakdown

### 1. Page Table Functions (paging.c)

#### create_user_page_table()

```c
Purpose: Create isolated virtual address space for user process

Input:  None
Output: Pointer to new page table (or NULL on error)

Steps:
1. Allocate physical page for root page table
2. Copy kernel entries (2-511) from kernel page table
3. Zero user entries (0-1) for unmapped user space
4. Return new page table
```

**Why this approach:**
- Isolation: Each process has own page table for memory protection
- Kernel sharing: All processes share kernel code/data via copied entries
- Efficiency: MMU uses single SATP per process
- Safety: User code cannot access kernel memory

#### map_user_code()

```c
Purpose: Map executable user code

Input:  page_table (user page table)
        user_vaddr (typically 0x10000)
        kernel_code (code source in kernel memory)
        size (code size)

Output: 0 on success, -1 on error

Steps:
1. Calculate number of pages needed
2. For each page:
   a. Allocate physical page via PMM
   b. Copy code from kernel_code to physical page
   c. Map with PTE_USER_TEXT (R, X, U)
3. Return success/failure
```

**Why separate from map_user_memory():**
- Semantics: Code is always read-only (enforced by permission bits)
- Safety: Helps prevent code injection
- Clarity: Explicit purpose reduces bugs

#### map_user_memory()

```c
Purpose: Map user data regions (stack, heap, etc.)

Input:  page_table (user page table)
        user_vaddr (virtual address)
        phys_addr (physical address, 0 = allocate)
        size (region size)
        writable (1 = RW, 0 = RO)

Output: 0 on success, -1 on error

Steps:
1. Calculate number of pages
2. Choose permissions (PTE_USER_DATA or PTE_USER_RO)
3. For each page:
   a. Allocate physical page if phys_addr==0
   b. Map virtual to physical with permissions
4. Return success/failure
```

**Flexibility:**
- Can allocate anonymous pages (phys_addr=0)
- Can map pre-allocated pages (phys_addr!=0)
- Can create read-only or read-write regions

#### switch_page_table()

```c
Purpose: Change active page table

Input:  page_table (new page table to use)

Output: None

Steps:
1. Calculate physical address of page table
2. Build SATP value:
   - Bits [63:60] = MODE (8 for Sv39)
   - Bits [59:44] = ASID (0, not used)
   - Bits [43:0] = PPN (physical page number)
3. Write to satp CSR
4. Flush TLB to ensure coherency
```

**Why TLB flush:**
- Old page table mappings may be cached in TLB
- New page table has different mappings
- TLB flush ensures next access fetches from new table

### 2. Process Creation (process.c)

#### process_create_user()

```c
Purpose: Create new user mode process

Input:  name (process name)
        user_code (code source in kernel)
        code_size (code size)

Output: Pointer to process (or NULL on error)

Steps:
1. Allocate process structure
2. Create user page table
3. Map user code at USER_CODE_BASE
4. Map user stack at USER_STACK_TOP
5. Allocate kernel stack for traps
6. Setup trap frame with user entry point
7. Add to scheduler
8. Return process
```

**Process Structure:**
```c
struct process {
    pid_t pid;                      // Process ID
    char name[PROC_NAME_LEN];       // Process name
    page_table_t *page_table;       // User page table (NULL = kernel)
    uintptr_t kernel_stack;         // Kernel stack for traps
    uintptr_t user_stack;           // User stack location
    struct trap_frame *trap_frame;  // Saved user state
    // ... other fields ...
};
```

#### user_mode_entry_wrapper()

```c
Purpose: Bridge kernel context to user mode

Input:  None (uses current_process)

Output: Never returns (enters user mode)

Steps:
1. Get current process
2. Switch page table to user page table
3. Setup sscratch with kernel stack
4. Call user_return() to enter user mode
```

**Why a wrapper:**
- Called from scheduler context
- Performs setup before user mode entry
- Handles process-specific initialization

### 3. Trap Handling (trap_entry.S)

#### Stack Switching Mechanism

```asm
trap_vector:
    # Atomic stack swap
    csrrw sp, sscratch, sp
    
    # Determine source mode
    beqz sp, trap_from_kernel
    
trap_from_user:
    # sp now = kernel stack
    # sscratch now = user stack
    addi sp, sp, -272
    csrr t0, sscratch
    sd t0, 16(sp)          # Save user sp in trap frame
    j save_registers
    
trap_from_kernel:
    # sp was 0, restore it
    csrrw sp, sscratch, sp
    addi sp, sp, -272
    # Calculate original sp
    # ... continue as kernel trap
```

**Why this works:**
- User mode: sscratch=kernel_sp, sp=user_sp
  - After swap: sp=kernel_sp (we get kernel stack!)
  - After swap: sscratch=user_sp (saved for later)
  - beqz fails (sp is not zero)
  
- Kernel mode: sscratch=0, sp=kernel_sp
  - After swap: sp=0 (we swapped with zero)
  - After swap: sscratch=kernel_sp (swapped content)
  - beqz succeeds (sp is zero)
  - We swap back to restore kernel sp

**Efficiency:**
- Only 1 CSR instruction for stack switch
- No register pressure
- No branch misprediction cost on non-exceptional path

### 4. User Mode Return (user_return.S)

```asm
user_return:
    mv t6, a0                    # Save trap_frame pointer
    
    # Restore exception PC
    ld t0, 248(t6)
    csrw sepc, t0
    
    # Restore status register
    ld t0, 256(t6)
    csrw sstatus, t0
    
    # Restore all registers
    ld ra, 8(t6)
    ld sp, 16(t6)
    # ... 30 more register restores ...
    ld t6, 248(t6)
    
    # Return to user mode
    sret
```

**Why pure assembly:**
- C compiler would generate prologue: allocate stack, save registers
- Prologue corrupts values before sret
- Pure assembly ensures exact register state at sret

**Register Offsets in trap_frame:**
```
Offset  Register      Offset  Register
0       sepc         136     a7
8       ra           144     s2
16      sp           152     s3
24      gp           160     s4
32      tp           168     s5
40      t0           176     s6
48      t1           184     s7
56      t2           192     s8
64      s0/fp        200     s9
72      s1           208     s10
80      a0           216     s11
88      a1           224     t3
96      a2           232     t4
104     a3           240     t5
112     a4           248     t6
120     a5           256     sstatus
128     a6
```

## Implementation Details

### Trap Frame Layout

The trap frame stores all registers and control state:

```c
struct trap_frame {
    unsigned long ra;       // 0: Return address
    unsigned long sp;       // 8: Stack pointer
    unsigned long gp;       // 16: Global pointer
    unsigned long tp;       // 24: Thread pointer
    unsigned long t0;       // 32: Temporary 0
    unsigned long t1;       // 40: Temporary 1
    unsigned long t2;       // 48: Temporary 2
    unsigned long s0;       // 56: Saved 0 / Frame pointer
    unsigned long s1;       // 64: Saved 1
    unsigned long a0;       // 72: Argument 0 / Return value
    unsigned long a1;       // 80: Argument 1 / Return value
    unsigned long a2;       // 88: Argument 2
    unsigned long a3;       // 96: Argument 3
    unsigned long a4;       // 104: Argument 4
    unsigned long a5;       // 112: Argument 5
    unsigned long a6;       // 120: Argument 6
    unsigned long a7;       // 128: Argument 7 / Syscall number
    unsigned long s2;       // 136: Saved 2
    unsigned long s3;       // 144: Saved 3
    unsigned long s4;       // 152: Saved 4
    unsigned long s5;       // 160: Saved 5
    unsigned long s6;       // 168: Saved 6
    unsigned long s7;       // 176: Saved 7
    unsigned long s8;       // 184: Saved 8
    unsigned long s9;       // 192: Saved 9
    unsigned long s10;      // 200: Saved 10
    unsigned long s11;      // 208: Saved 11
    unsigned long t3;       // 216: Temporary 3
    unsigned long t4;       // 224: Temporary 4
    unsigned long t5;       // 232: Temporary 5
    unsigned long t6;       // 240: Temporary 6
    unsigned long sepc;     // 248: Exception program counter
    unsigned long sstatus;  // 256: Supervisor status register
};
```

**Size:** 264 bytes (33 fields × 8 bytes)

**Alignment:** 8-byte aligned (required by ABI)

### Page Table Entry Format

Each PTE is a 64-bit value:

```
[63:54]  Reserved  (ignored)
[53:10]  PPN       (Physical Page Number - 44 bits)
[9:8]    Reserved  (ignored)
[7:0]    Flags     (permission bits)

Flags:
  [0]  V (Valid)       - Entry is valid
  [1]  R (Read)        - Page is readable
  [2]  W (Write)       - Page is writable
  [3]  X (Execute)     - Page is executable
  [4]  U (User)        - Accessible in user mode
  [5]  G (Global)      - TLB entry is global
  [6]  A (Accessed)    - Page has been accessed
  [7]  D (Dirty)       - Page has been written to
```

**Permission Combinations:**
```c
// User-accessible (U bit set)
PTE_USER_TEXT   = V | R | X | U              // Code
PTE_USER_DATA   = V | R | W | U              // Stack/Heap
PTE_USER_RO     = V | R | U                  // Read-only

// Kernel-only (no U bit)
PTE_KERNEL_DATA = V | R | W | X              // Kernel memory
```

### SATP Register (Supervisor Address Translation and Protection)

```
[63:60] MODE    - Paging mode (8 = Sv39)
[59:44] ASID    - Address Space ID (ignored, set to 0)
[43:0]  PPN     - Physical Page Number of root table
```

**Writing SATP:**
```c
uint64_t satp = (8UL << 60) | (root_page_table_pa >> 12);
asm volatile("csrw satp, %0" :: "r"(satp));
```

## Testing Strategy

### Unit Tests

1. **Page Table Creation**
   - Verify kernel entries are copied
   - Verify user entries are zero
   - Verify memory is allocated

2. **Code Mapping**
   - Verify code is at correct address
   - Verify code is copied correctly
   - Verify permissions are set (R, X, U)

3. **Stack Mapping**
   - Verify stack is allocated
   - Verify stack permissions (R, W, U)
   - Verify stack grows downward

4. **Page Table Switching**
   - Verify SATP is updated
   - Verify TLB is flushed
   - Verify kernel still accessible

### Integration Tests

1. **User Process Creation**
   - Process structure initialized
   - Page table created
   - Code and stack mapped
   - Process appears in process table

2. **Trap Entry/Exit**
   - Stack switching works
   - Trap frame saved/restored correctly
   - Kernel stack preserved

3. **Mode Transitions**
   - Can enter user mode via sret
   - Can trap from user mode
   - Can return to user mode

### Test Code (tests/user_test.c)

```c
uint8_t user_code[] = {
    // lui sp, 0x80000      - Load stack pointer
    // addi sp, sp, 0
    // ecall                - System call (will fail for now)
};

struct process *proc = process_create_user("test", user_code, sizeof(user_code));
if (proc && proc->pid == expected_pid) {
    hal_uart_puts("[PASS] User process created\n");
}
```

## Troubleshooting

### Issue: User process not created

**Symptoms:**
- process_create_user() returns NULL
- Error message: "Failed to create page table"

**Causes:**
1. PMM out of memory
2. kmalloc failure
3. Page table allocation failure

**Solution:**
1. Check free memory: `pmm_get_stats()`
2. Verify PMM is initialized before process creation
3. Check for memory leaks

### Issue: Trap from user mode hangs

**Symptoms:**
- System freezes when user code executes
- No console output

**Causes:**
1. Stack swap failed (sscratch not set correctly)
2. Kernel stack is invalid
3. Trap handler infinite loop

**Solution:**
1. Verify sscratch is set before entering user mode
2. Check kernel stack alignment (must be 16-byte aligned)
3. Add debug output in trap_entry.S

### Issue: Wrong privilege level after sret

**Symptoms:**
- User code executes in kernel mode
- Or kernel code executes in user mode

**Causes:**
1. SPP bit not set correctly in sstatus
2. Trap frame corrupted
3. Register restore failed

**Solution:**
1. Verify SPP=0 (user mode) in sstatus
2. Check trap frame at offset 256 for sstatus value
3. Verify user_return restores sepc correctly

### Issue: Page fault from user code

**Symptoms:**
- Page fault exception when user code tries to access memory
- Cause: Load/store/instruction page fault

**Causes:**
1. Memory not mapped for user
2. Insufficient permissions on page
3. Virtual address outside user space

**Solution:**
1. Check if memory is mapped: virt_to_phys()
2. Verify permissions (PTE_U bit set)
3. Verify address is in user space (< 0x80000000)

---

This implementation guide provides the architectural foundation for extending user mode with system calls, signals, and other OS features.
