# GitHub Copilot Code Review Instructions for ThunderOS

## Project Context

ThunderOS is a RISC-V operating system implementing:
- POSIX-inspired errno error handling system
- ext2 filesystem support with VirtIO block devices
- ELF binary loading and execution
- Virtual memory management (Sv39 paging)
- Process scheduling and syscalls
- Interactive shell interface

## Critical Review Areas

### 1. Error Handling (errno System)

**What to Check:**
- ✅ All functions that can fail MUST set errno with appropriate error code
- ✅ Use semantic error codes (e.g., `THUNDEROS_EINVAL`, `THUNDEROS_ENOMEM`) NOT magic numbers
- ✅ Functions should call `clear_errno()` on success path
- ✅ Error propagation: if calling function that sets errno, preserve it (don't overwrite)
- ✅ Use helper macros: `RETURN_ERRNO()`, `RETURN_ERRNO_NULL()`, `SET_ERRNO_GOTO()`

**Common Mistakes:**
- ❌ Returning `-1` without setting errno
- ❌ Using magic numbers like `-2`, `-3` for different errors
- ❌ Setting errno on success (should always clear it)
- ❌ Overwriting errno from called functions

**Example - Good:**
```c
int vfs_open(const char *path, uint32_t flags) {
    if (!path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        RETURN_ERRNO(THUNDEROS_ENOENT);  // Set specific error
    }
    
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        // errno already set by vfs_alloc_fd, just return
        return -1;
    }
    
    clear_errno();  // Clear on success
    return fd;
}
```

**Example - Bad:**
```c
int vfs_open(const char *path, uint32_t flags) {
    if (!path) {
        return -1;  // ❌ What error? No errno set!
    }
    
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        return -2;  // ❌ Magic number! Should use errno
    }
    
    return fd;  // ❌ Didn't clear errno on success
}
```

### 2. Memory Management

**What to Check:**
- ✅ All `kmalloc()` results checked for NULL before use
- ✅ Memory freed with `kfree()` when no longer needed
- ✅ DMA memory allocated with `dma_alloc()` for VirtIO operations
- ✅ No memory leaks in error paths (use goto cleanup pattern)
- ✅ Virtual addresses translated to physical for hardware access

**Common Mistakes:**
- ❌ Using `kmalloc()` memory for VirtIO (needs DMA memory)
- ❌ Not checking return value of `kmalloc()`
- ❌ Memory leaks in early return/error paths
- ❌ Double free

**Example - Good:**
```c
int operation(void) {
    void *buffer = NULL;
    int result = -1;
    
    buffer = kmalloc(4096);
    if (!buffer) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    if (do_work(buffer) < 0) {
        // errno already set
        goto cleanup;
    }
    
    result = 0;
    clear_errno();
    
cleanup:
    if (buffer) kfree(buffer);
    return result;
}
```

### 3. VirtIO Block Device

**What to Check:**
- ✅ All VirtIO operations use DMA-allocated memory
- ✅ Physical addresses used for descriptor addresses (not virtual)
- ✅ Proper timeout handling with `THUNDEROS_EVIRTIO_TIMEOUT` error
- ✅ Memory barriers (`mb()`, `rmb()`, `wmb()`) around device access
- ✅ Descriptor chains properly linked (idx0 → idx1 → idx2)

**Common Mistakes:**
- ❌ Using virtual addresses in VirtIO descriptors
- ❌ Using kmalloc instead of dma_alloc for buffers
- ❌ Missing memory barriers
- ❌ Incorrect descriptor flags (NEXT, WRITE)

### 4. ext2 Filesystem

**What to Check:**
- ✅ Block numbers validated against superblock limits
- ✅ Inode numbers validated (must be ≥ 1, ≤ s_inodes_count)
- ✅ NULL pointer checks before dereferencing structures
- ✅ Error codes: `THUNDEROS_EFS_BADBLK`, `THUNDEROS_EFS_BADINO`, `THUNDEROS_EFS_CORRUPT`
- ✅ Superblock magic number checked (0xEF53)

**Common Mistakes:**
- ❌ Not validating block/inode numbers
- ❌ Assuming operations succeed without checking return values
- ❌ Off-by-one errors in block group calculations
- ❌ Not handling read/write errors properly

### 5. VFS Layer

**What to Check:**
- ✅ Path validation (must start with `/`)
- ✅ File descriptor validation before use
- ✅ Filesystem operations check if root filesystem mounted
- ✅ Proper error codes: `THUNDEROS_ENOENT`, `THUNDEROS_EBADF`, `THUNDEROS_EINVAL`
- ✅ Reference counting for vnodes (if implemented)

### 6. Process Management

**What to Check:**
- ✅ Process limit checks (`MAX_PROCESSES`)
- ✅ PID validation before accessing process table
- ✅ User/kernel mode transitions properly handled
- ✅ Per-process errno properly isolated
- ✅ Process state transitions valid

### 7. RISC-V Specific

**What to Check:**
- ✅ Trap handlers preserve all registers
- ✅ Supervisor/User mode transitions use proper CSR instructions
- ✅ Page table entries properly aligned (4KB)
- ✅ Virtual addresses canonical (sign-extended for Sv39)
- ✅ Atomic operations where needed for concurrent access

## Code Style Requirements

### Naming Conventions
- Functions: `snake_case` (e.g., `vfs_open`, `ext2_read_inode`)
- Macros: `SCREAMING_SNAKE_CASE` (e.g., `THUNDEROS_EINVAL`, `MAX_PROCESSES`)
- Structs: `snake_case` with `_t` suffix (e.g., `vfs_node_t`, `ext2_fs_t`)
- Global variables: `g_` prefix (e.g., `g_root_fs`, `g_process_table`)

### Error Handling Pattern
```c
int function(params) {
    // Validate parameters
    if (!param) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    // Allocate resources
    resource = allocate();
    if (!resource) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    // Perform operations
    if (operation() < 0) {
        // errno already set by operation()
        goto cleanup;
    }
    
    // Success path
    clear_errno();
    return 0;
    
cleanup:
    // Free resources
    free_resource(resource);
    return -1;
}
```

### Documentation Requirements
- All public functions need comment describing purpose, parameters, return value
- Complex algorithms need explanatory comments
- Error codes should be documented in function comment

## Testing Requirements

### Before Approving PR

1. **Build Test:**
   ```bash
   make clean && make
   ```
   Should build with no errors, warnings acceptable only for known issues.

2. **QEMU Test (Critical!):**
   ```bash
   # Create test filesystem
   cd build
   mkdir -p testfs && echo "test" > testfs/test.txt
   mkfs.ext2 -F -q -d testfs fs.img 10M
   cd ..
   
   # Run with correct QEMU flags
   qemu-system-riscv64 \
       -machine virt \
       -m 128M \
       -nographic \
       -serial mon:stdio \
       -bios default \
       -kernel build/thunderos.elf \
       -global virtio-mmio.force-legacy=false \
       -drive file=build/fs.img,if=none,format=raw,id=hd0 \
       -device virtio-blk-device,drive=hd0
   ```
   
   **Must verify:**
   - ✅ Kernel boots successfully
   - ✅ Memory tests pass (10/10)
   - ✅ VirtIO device initializes
   - ✅ ext2 filesystem mounts (NOT "Failed to mount")
   - ✅ Shell becomes interactive
   - ✅ Can run `ls` and `cat /hello.txt`

3. **errno Tests (if modified error handling):**
   - Check that errno tests run and pass
   - Verify ext2 error tests run (not skipped)
   - Confirm all test output shows PASS

### Red Flags in PR

- ❌ Changes to VirtIO without testing with actual disk image
- ❌ Modified ext2 code without verifying filesystem mounts
- ❌ New error returns without setting errno
- ❌ Memory allocation without NULL check
- ❌ Missing cleanup in error paths
- ❌ Hardcoded magic numbers instead of named constants
- ❌ Changes to QEMU invocation removing `-global virtio-mmio.force-legacy=false`

## Common QEMU Issues

### VirtIO Timeout / Failed Mount

**Symptom:** `[FAIL] Failed to mount ext2 filesystem` or `virtio_blk_do_request: Timeout`

**Cause:** Missing `-global virtio-mmio.force-legacy=false` flag

**Fix:** Always use this flag when running QEMU with VirtIO devices. This enables modern VirtIO mode. Without it, all I/O operations timeout.

**Correct Command:**
```bash
qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/fs.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0
```

## Review Checklist

For each PR, verify:

- [ ] Code builds without errors
- [ ] All modified subsystems tested in QEMU
- [ ] errno properly set for all error paths
- [ ] Memory allocations checked and freed
- [ ] No magic numbers (use named constants)
- [ ] Comments added for complex logic
- [ ] VirtIO operations use DMA memory
- [ ] ext2 operations validate block/inode numbers
- [ ] VFS operations check filesystem mounted
- [ ] Process operations validate PIDs
- [ ] No regression in existing tests
- [ ] New functionality has tests (if applicable)
- [ ] Documentation updated (if API changed)
- [ ] Commit messages follow standards (see below)

## Git Commit Message Standards

**Format Requirements:**

- ✅ **Short messages** - Keep under 72 characters for subject line
- ✅ **Past participle** - Use verb forms like "Added", "Fixed", "Updated", "Implemented", "Removed"
- ✅ **No prefixes** - Don't use "feat:", "fix:", "docs:", "refactor:", etc.
- ✅ **Descriptive** - Explain what changed, not how

**Good Examples:**

```
Added errno error handling to VFS layer

Fixed VirtIO timeout issue in block driver

Implemented ext2 directory traversal

Updated documentation for QEMU configuration

Removed deprecated magic number error codes
```

**Bad Examples:**

```
fix: fixing the bug        ❌ Uses "fix:" prefix
Add support for errno      ❌ Not past participle
docs: updated readme       ❌ Uses "docs:" prefix
Changed some stuff         ❌ Not descriptive
feat: implement vfs open   ❌ Uses "feat:" prefix, not past participle
```

**Multi-line Commits:**

For complex changes, add a blank line and detailed explanation:

```
Converted ext2 filesystem to use errno error handling

- Replaced magic numbers with semantic error codes
- Added THUNDEROS_EFS_* error codes for filesystem errors
- Updated all error paths to call set_errno()
- Validated block and inode numbers with EINVAL/EFS_BADINO
- Ensured errno cleared on success paths
```

## Priority Issues

**P0 - Block PR:**
- Build failures
- Kernel panic/crash in QEMU
- Memory corruption/leaks
- Returning errors without setting errno
- Using virtual addresses for DMA/VirtIO
- Commit messages not following standards

**P1 - Must fix before merge:**
- Missing NULL checks on allocations
- Missing error cleanup paths
- Incorrect errno values for errors
- Magic numbers instead of constants

**P2 - Should fix:**
- Missing function documentation
- Unclear variable names
- Excessive code duplication
- Minor style violations

**P3 - Nice to have:**
- Additional comments for clarity
- Performance optimizations
- Code refactoring for readability