# ThunderOS User Shell Debugging Journal

## Status: ✅ WORKING (Nov 28, 2025)

The user-mode shell is now fully functional! All commands work correctly including fork+exec.

---

## Working Commands
- `help` - ✅ Shows available commands
- `echo <text>` - ✅ Echoes arguments  
- `cat <file>` - ✅ Display file contents
- `hello` - ✅ Fork+exec hello program, returns to shell correctly
- `exit` - ✅ Exit shell

---

## Bugs Fixed This Session

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

