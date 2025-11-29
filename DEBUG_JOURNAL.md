# ThunderOS User Shell Debugging Journal

## Status: 🔧 DEBUGGING (Nov 29, 2025)

VT2 shell doesn't respond to input after switching from VT1.

---

## Current Bug: VT2 terminal unresponsive (Nov 29, 2025)

### Status: 🔧 DEBUGGING

### Symptom
- VT1 shell works fine
- Switch to VT2 with ESC+2
- VT2 shows shell prompt but doesn't respond to any key presses
- Switching back to VT1 works, clock continues

### What We Tried
1. Changed `sys_read` to use non-blocking UART with `process_yield()` instead of 
   spinning in `hal_uart_getc()` - didn't fix VT2
2. Added terminal switch detection in the read loop - didn't fix VT2

### Hypothesis
VT2's shell process may be stuck somewhere or not getting scheduled properly.
OR the input routing to VT2 is broken.

### Key Code Path for Input
1. Timer interrupt calls `vterm_poll_input()` which reads UART and buffers to active terminal
2. `sys_read` for stdin checks `vterm_has_buffered_input_for(tty)`
3. If buffered input available, return it
4. Otherwise wait for active terminal and read UART directly

### Things to Check
- [ ] Is VT2 shell even running? (Check process state)
- [ ] Is input being buffered to VT2's buffer correctly?
- [ ] Is VT2 shell stuck in `sys_read` yield loop?
- [ ] Is there a race between timer polling and direct UART read?

---

## Fixed: VT1 clock hangs when switching back from VT2 (Nov 29, 2025)

### Status: ✅ FIXED

### Root Cause
Three issues were found:

1. **Missing page table switch in forked_child_entry**: The child was running with
   the parent's page table instead of its own. Fixed by calling `switch_page_table()`
   in `forked_child_entry` before entering user mode.

2. **Missing SUM bit in trap handler**: When a trap occurs, the kernel needs to
   access user memory (e.g., reading path strings for exec syscall). RISC-V requires
   the SUM (Supervisor User Memory) bit in sstatus to be set for this.
   
   The trap entry was NOT setting SUM before calling the C trap handler, causing
   page faults when syscall handlers tried to access user buffers.

3. **Use-after-free in elf_exec_replace**: The `path` argument points to user memory
   (e.g., shell's `.rodata` section). The exec code would:
   - Open the file using `path` (works fine)
   - Load the new ELF binary
   - **Free the old process's memory pages** (including where `path` lives!)
   - Try to extract program name from `path` → **CRASH!**
   
   Fixed by copying `path` to a kernel buffer at the start of `elf_exec_replace`.

### Fixes Applied

**trap_entry.S**: Set SUM bit after saving sstatus:
```asm
li t0, (1 << 18)
csrs sstatus, t0
```

**elf_loader.c**: Copy path to kernel buffer before freeing user memory:
```c
char path_buf[256];
size_t path_len = 0;
while (path[path_len] && path_len < sizeof(path_buf) - 1) {
    path_buf[path_len] = path[path_len];
    path_len++;
}
path_buf[path_len] = '\0';
const char *kpath = path_buf;  // Use this from now on
```

### Symptom (before fix)
When running clock on VT1 and then switching to VT2 to run commands:
- Process 4 (ush on VT2) gets "Instruction page fault"
- Process 5 (ush on VT2 respawned?) also crashes
- sepc = 0x105ec, stval = 0x105ec (same address)
- s0 = 0x40000000 (exactly at user space boundary - suspicious!)

### Test Output
```
[OK] Shell on VT1 (PID 1)
[OK] Shell on VT2 (PID 2)
[VT] Switched to VT2

!!! USER PROCESS EXCEPTION !!!
Process: 4 (ush)
Exception: Instruction page fault
sepc:   0x00000000000105ec
stval:  0x00000000000105ec
ra:     0x0000000000010060
sp:     0x000000003fffeb70
s0:     0x0000000040000000  <-- SUSPICIOUS! Exactly at boundary
gp:     0x00000000000111d3
a0-a2:  0x0000000000000000 0x0000000000010940 0x000000003fffeff0
```

### Analysis
1. **Process IDs:** VT1 shell = PID 1, VT2 shell = PID 2
2. **Crashing process:** PID 4 is a forked child (ush forked to run a command)
3. **s0 = 0x40000000:** This is USER_SPACE_END, the frame pointer is corrupted
4. **sepc = 0x105ec:** Attempting to execute at this address fails

### Hypotheses
1. **Frame pointer corruption during fork:** Child inherits corrupted s0
2. **Stack/VM overlap:** Multiple processes may have conflicting VM mappings
3. **Kernel stack exhaustion:** Deep nesting of syscalls/interrupts
4. **sscratch issue with nested processes:** More than 2 processes causes confusion

### Previous Fix Applied
Fixed sys_sleep to save/restore sscratch before enabling interrupts:
```c
uint64_t saved_sscratch;
__asm__ volatile("csrrw %0, sscratch, zero" : "=r"(saved_sscratch));
__asm__ volatile("csrs sstatus, %0" :: "r"(1UL << 1));
// ... sleep loop ...
__asm__ volatile("csrc sstatus, %0" :: "r"(1UL << 1));
__asm__ volatile("csrw sscratch, %0" :: "r"(saved_sscratch));
```

This fixed the KERNEL exception, but now user processes crash.

### Next Steps
- [ ] Check fork() to see if child inherits correct register state
- [ ] Check if process kernel stacks are properly isolated
- [ ] Look for 0x40000000 constant being used incorrectly
- [ ] Verify page table isolation between processes

---

## Fixed: Kernel exception during sys_sleep (Nov 29, 2025)

### Root Cause
When sys_sleep enables interrupts (csrs sstatus, SIE) and a timer interrupt fires:
- Trap entry does `csrrw sp, sscratch, sp`
- If sscratch still contains user's kernel stack pointer, trap thinks we came from user mode
- Stack corruption and kernel crash ensues

### Solution
Save and clear sscratch before enabling interrupts in sys_sleep:
```c
uint64_t saved_sscratch;
__asm__ volatile("csrrw %0, sscratch, zero" : "=r"(saved_sscratch));
// ... enable interrupts, sleep, disable interrupts ...
__asm__ volatile("csrw sscratch, %0" :: "r"(saved_sscratch));
```

---

## Fixed: sys_sleep hangs (Nov 29, 2025)

---

## Fixed: sys_sleep hangs (Nov 29, 2025)

### Root Cause
When a syscall trap occurs, RISC-V hardware automatically clears `sstatus.SIE` (supervisor interrupt enable). This means during `sys_sleep`'s busy-wait loop, timer interrupts could NOT fire, so `hal_timer_get_ticks()` never incremented.

### Solution
Enable interrupts during the sleep loop in `sys_sleep`:
```c
uint64_t sys_sleep(uint64_t milliseconds) {
    // ... setup ...
    
    /* Enable interrupts so timer can fire during sleep */
    asm volatile("csrsi sstatus, 0x2");  // Set SIE bit
    
    /* Wait for timer ticks */
    while (hal_timer_get_ticks() < target_ticks) {
        asm volatile("wfi");  // Wait for interrupt
    }
    
    /* Disable interrupts before returning */
    asm volatile("csrci sstatus, 0x2");  // Clear SIE bit
    
    return SYSCALL_SUCCESS;
}
```

### Result
Clock now works correctly, printing incrementing seconds:
```
ush> clock
=== Clock Started ===
1
2
3
...
```

---

## Current Debug Session: Console Multiplexing & Clock Program

### Goal
Implement console multiplexing so each process can output to its own VT, and create a clock program to test it.

### What Was Implemented
1. **Console Multiplexing**
   - Per-process `controlling_tty` field in process struct
   - `vterm_putc_to(int tty, char c)` - write to specific VT
   - UART-only mode for vterm (no GPU required)
   - Input polling in timer interrupt for VT switching

2. **Clock Program** (`userland/clock.c`)
   - Prints elapsed seconds every second
   - Uses `sys_gettime()` and `sys_sleep(100)`

3. **Timer/Sleep Fixes**
   - Fixed `sys_gettime()` - was dividing by TICKS_PER_MS (10000) but ticks are 100ms intervals
   - Fixed to: `return ticks * 100;` (each tick = 100ms)
   - Fixed `sys_sleep()` - was multiplying by TICKS_PER_MS
   - Fixed to: `ticks_to_wait = (milliseconds + 99) / 100;`

### Current Bug: sys_sleep hangs
**Symptom:** Clock prints "Sleeping 100ms..." then hangs forever
**Test output:**
```
ush> clock
=== Clock Started ===
Start time: 400 ms
Sleeping 100ms...
[hangs forever]
```

**Analysis:**
- `sys_sleep(100)` calls `process_yield()` in a loop
- `process_yield()` calls `schedule()`
- Clock is forked from shell, shell is waiting in `waitpid`
- Only two processes: shell (blocked) and clock (yielding)
- When clock yields, scheduler should pick it back up

**Hypothesis:** Clock yields but never gets scheduled again because:
1. Shell is blocked in waitpid (state != PROC_RUNNING)
2. Clock yields and is enqueued
3. Scheduler picks clock... but something goes wrong

**Next Steps to Try:**
- [ ] Check if clock is being enqueued correctly after yield
- [ ] Check scheduler_pick_next() logic
- [ ] Add debug output to schedule() to trace what's happening
- [ ] Check if clock's state is correct after yield

---

## Previous Session: Working Shell (Nov 28, 2025)

The user-mode shell is fully functional! All commands work correctly including fork+exec.

---

## Working Commands
- `help` - ✅ Shows available commands
- `echo <text>` - ✅ Echoes arguments  
- `cat <file>` - ✅ Display file contents
- `hello` - ✅ Fork+exec hello program, returns to shell correctly
- `exit` - ✅ Exit shell

---

## Bugs Fixed (Nov 28)

### Bug 1: sscratch not restored on trap return to user mode
**File:** `kernel/arch/riscv64/trap_entry.S`
**Symptom:** Shell's context.sp saved as USER stack (0x3FFFFA10) instead of kernel stack
**Root Cause:** After returning to user mode, sscratch contained USER stack instead of kernel stack. Next trap would swap sp with wrong value.
**Fix:** In `restore_to_user`, save kernel stack top to sscratch BEFORE loading user sp:
```asm
addi t6, sp, 272      # Calculate kernel stack top
csrw sscratch, t6     # Save for next trap
ld sp, 8(sp)          # Now load user sp
sret
```

### Bug 2: Deadlock in process_exit
**File:** `kernel/core/process.c`
**Symptom:** System hangs after "[EXIT] Marked as zombie"
**Root Cause:** process_exit held process_lock, then called signal_send → process_wakeup which tried to acquire same lock.
**Fix:** Move signal_send() call to AFTER lock_release().

### Bug 3: Page table not switched after context switch return
**File:** `kernel/core/scheduler.c`  
**Symptom:** Load page fault accessing user memory (stval=0x3fff7ff8) after context_switch_asm returns
**Root Cause:** When switching from process A to B, then back to A, the page table remained B's. Process A then tried to access its user memory which wasn't mapped.
**Fix:** Add page table switch after context_switch_asm returns:
```c
struct process *current_after = process_current();
if (current_after && current_after->page_table) {
    switch_page_table(current_after->page_table);
}
```

### Bug 4: kfree called on user stack address
**File:** `kernel/core/process.c`
**Symptom:** kfree crash at address 0x3fff7ff8 (user stack region)
**Root Cause:** process_free() was calling kfree(proc->user_stack), but user_stack is a USER SPACE virtual address, not a kmalloc allocation.
**Fix:** Remove the kfree(proc->user_stack) call - user stack pages are freed when freeing page table.

---

## Architecture Notes

### Context Switch Flow
1. `context_switch_asm` saves old process ra/sp/s0-s11
2. Loads new process ra/sp/s0-s11
3. Returns to address in new ra
4. After return, switch to current process's page table

### Page Table Ownership
- Each process has its own page table with user mappings
- Kernel mappings (entries[2-511]) are shared/copied
- Must switch page table when switching between processes

### Entry Points After Context Switch
1. **user_mode_entry_wrapper** - Initial process start, switches PT itself
2. **forked_child_entry** - Child after fork, switches PT in exec
3. **Return from yield** - Process resuming from scheduler_yield(), PT switched in context_switch

### Trap Entry/Exit Flow (User Mode)
1. Entry: `csrrw sp, sscratch, sp` - swap sp and sscratch
2. sp now = kernel stack, sscratch = user stack
3. Allocate trap frame, save registers, call handler
4. Exit: Save kernel stack top to sscratch, load user sp, sret

---

## Files Modified
- `kernel/arch/riscv64/trap_entry.S` - Fixed sscratch restore
- `kernel/core/process.c` - Fixed deadlock in process_exit, removed bad kfree
- `kernel/core/scheduler.c` - Added page table switch after context switch
- `kernel/mm/paging.c` - Cleanup

