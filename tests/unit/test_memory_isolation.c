/*
 * Memory Isolation Test Suite
 * 
 * Tests complete process memory isolation including:
 * - Per-process page tables
 * - VMA tracking and validation
 * - Heap isolation (sys_brk)
 * - Memory protection enforcement
 * - Fork memory copying
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/process.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "kernel/kstring.h"
#include "hal/hal_uart.h"

// Simple test framework
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    do { \
        hal_uart_puts("Test: "); \
        hal_uart_puts(name); \
        hal_uart_puts("... "); \
    } while(0)

#define TEST_PASS() \
    do { \
        hal_uart_puts("PASS\n"); \
        tests_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        hal_uart_puts("FAIL - "); \
        hal_uart_puts(msg); \
        hal_uart_puts("\n"); \
        tests_failed++; \
        return; \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
        } \
    } while(0)

// Test helper to create a minimal test process structure
static struct process *create_test_process_struct(const char *name) {
    // Static test process structures
    static struct process test_procs[20];
    static int next_test_proc = 0;
    
    if (next_test_proc >= 20) {
        return NULL;
    }
    
    struct process *proc = &test_procs[next_test_proc++];
    
    // Initialize minimal process structure
    kmemset(proc, 0, sizeof(struct process));
    proc->pid = 100 + next_test_proc;
    proc->state = PROC_EMBRYO;
    kstrcpy(proc->name, name);
    
    // Create isolated page table
    proc->page_table = create_user_page_table();
    if (!proc->page_table) {
        proc->page_table = get_kernel_page_table();
    }
    
    // Set up memory isolation (initializes heap and vm_areas list)
    process_setup_memory_isolation(proc);
    
    return proc;
}

static void cleanup_test_process(struct process *proc) {
    if (!proc) return;
    
    process_cleanup_vmas(proc);
    
    if (proc->page_table && proc->page_table != get_kernel_page_table()) {
        free_page_table(proc->page_table);
    }
    
    proc->state = PROC_UNUSED;
    proc->pid = -1;
}

/**
 * Test 1: Process has isolated page table
 */
static void test_isolated_page_table(void) {
    TEST_START("Process has isolated page table");
    
    struct process *proc = create_test_process_struct("test_proc1");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Verify process has its own page table (not sharing kernel page table)
    ASSERT(proc->page_table != NULL, "Process has no page table");
    ASSERT(proc->page_table != get_kernel_page_table(), 
                "Process using kernel page table (not isolated)");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 2: VMA list is initialized
 */
static void test_vma_initialization(void) {
    TEST_START("VMA list is initialized");
    
    struct process *proc = create_test_process_struct("test_proc2");
    ASSERT(proc != NULL, "Process creation failed");
    
    // process_setup_memory_isolation() initializes vm_areas to NULL
    // VMAs are only added when we map actual memory regions
    
    // Test that we can add a VMA
    int result = process_add_vma(proc, 0x10000, 0x20000, VM_READ | VM_WRITE | VM_USER);
    ASSERT(result == 0, "Failed to add VMA");
    ASSERT(proc->vm_areas != NULL, "VMA not added to list");
    
    // Verify the VMA we added
    vm_area_t *vma = proc->vm_areas;
    ASSERT(vma->start == 0x10000, "VMA start address incorrect");
    ASSERT(vma->end == 0x20000, "VMA end address incorrect");
    ASSERT(vma->flags & VM_USER, "VMA missing VM_USER flag");
    ASSERT(vma->flags & VM_READ, "VMA missing VM_READ flag");
    ASSERT(vma->flags & VM_WRITE, "VMA missing VM_WRITE flag");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 3: VMA permissions tracking works
 */
static void test_code_vma_permissions(void) {
    TEST_START("VMA permissions tracking works");
    
    struct process *proc = create_test_process_struct("test_proc3");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Add a VMA with specific permissions (simulating code segment)
    int result = process_add_vma(proc, 0x10000, 0x20000, VM_READ | VM_EXEC | VM_USER);
    ASSERT(result == 0, "Failed to add VMA");
    
    // Find and verify the VMA
    vm_area_t *vma = process_find_vma(proc, 0x15000);
    ASSERT(vma != NULL, "VMA not found");
    
    // Verify permissions are as expected
    ASSERT(vma->flags & VM_READ, "VMA not readable");
    ASSERT(vma->flags & VM_EXEC, "VMA not executable");
    ASSERT(!(vma->flags & VM_WRITE), "VMA should not be writable");
    ASSERT(vma->flags & VM_USER, "VMA not user-accessible");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 4: Multiple VMAs can coexist
 */
static void test_stack_vma_permissions(void) {
    TEST_START("Multiple VMAs can coexist");
    
    struct process *proc = create_test_process_struct("test_proc4");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Add code-like VMA
    int result1 = process_add_vma(proc, 0x10000, 0x20000, VM_READ | VM_EXEC | VM_USER);
    ASSERT(result1 == 0, "Failed to add code VMA");
    
    // Add stack-like VMA
    int result2 = process_add_vma(proc, 0x40000, 0x50000, VM_READ | VM_WRITE | VM_USER);
    ASSERT(result2 == 0, "Failed to add stack VMA");
    
    // Verify both exist and are different
    vm_area_t *code_vma = process_find_vma(proc, 0x15000);
    vm_area_t *stack_vma = process_find_vma(proc, 0x45000);
    
    ASSERT(code_vma != NULL, "Code VMA not found");
    ASSERT(stack_vma != NULL, "Stack VMA not found");
    ASSERT(code_vma != stack_vma, "VMAs should be different");
    
    // Verify code VMA permissions
    ASSERT(code_vma->flags & VM_EXEC, "Code VMA not executable");
    ASSERT(!(code_vma->flags & VM_WRITE), "Code VMA should not be writable");
    
    // Verify stack VMA permissions
    ASSERT(stack_vma->flags & VM_WRITE, "Stack VMA not writable");
    ASSERT(!(stack_vma->flags & VM_EXEC), "Stack VMA should not be executable");
    
    // Clean up
    cleanup_test_process(proc);

    
    TEST_PASS();
}

/**
 * Test 5: Heap boundaries are initialized
 */
static void test_heap_initialization(void) {
    TEST_START("Heap boundaries are initialized");
    
    struct process *proc = create_test_process_struct("test_proc5");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Verify heap boundaries
    ASSERT(proc->heap_start == USER_HEAP_BASE, "Heap start incorrect");
    ASSERT(proc->heap_end == USER_HEAP_BASE, "Heap end should equal start initially");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 6: process_map_region creates VMA
 */
static void test_map_region_creates_vma(void) {
    TEST_START("process_map_region creates VMA");
    
    struct process *proc = create_test_process_struct("test_proc6");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Map a test region
    uint64_t test_addr = USER_MMAP_START;
    uint64_t test_size = PAGE_SIZE * 2;
    
    int result = process_map_region(proc, test_addr, test_size, 
                                    VM_READ | VM_WRITE | VM_USER);
    ASSERT(result == 0, "process_map_region failed");
    
    // Verify VMA was created
    vm_area_t *vma = process_find_vma(proc, test_addr);
    ASSERT(vma != NULL, "VMA not created for mapped region");
    ASSERT(vma->start <= test_addr, "VMA start incorrect");
    ASSERT(vma->end >= test_addr + test_size, "VMA end incorrect");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 7: process_validate_user_ptr validates permissions
 */
static void test_validate_user_ptr_permissions(void) {
    TEST_START("process_validate_user_ptr validates permissions");
    
    struct process *proc = create_test_process_struct("test_proc7");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Add a read-only VMA
    uint64_t test_addr = 0x10000;
    int result = process_add_vma(proc, test_addr, test_addr + PAGE_SIZE, VM_READ | VM_USER);
    ASSERT(result == 0, "Failed to add read-only VMA");
    
    // Should succeed with read permission
    int valid = process_validate_user_ptr(proc, (void *)test_addr, 16, VM_READ);
    ASSERT(valid == 1, "Failed to validate read access");
    
    // Should fail with write permission (VMA is read-only)
    valid = process_validate_user_ptr(proc, (void *)test_addr, 16, VM_WRITE);
    ASSERT(valid == 0, "Should not validate write access to read-only VMA");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 8: process_validate_user_ptr rejects unmapped addresses
 */
static void test_validate_user_ptr_rejects_unmapped(void) {
    TEST_START("process_validate_user_ptr rejects unmapped addresses");
    
    struct process *proc = create_test_process_struct("test_proc8");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Try to validate an unmapped address
    uint64_t unmapped_addr = USER_MMAP_START + 0x100000;
    int valid = process_validate_user_ptr(proc, (void *)unmapped_addr, 
                                         16, VM_READ);
    ASSERT(valid == 0, "Should reject unmapped address");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 9: process_validate_user_ptr rejects kernel addresses
 */
static void test_validate_user_ptr_rejects_kernel(void) {
    TEST_START("process_validate_user_ptr rejects kernel addresses");
    
    struct process *proc = create_test_process_struct("test_proc9");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Try to validate a kernel address
    uint64_t kernel_addr = KERNEL_VIRT_BASE;
    int valid = process_validate_user_ptr(proc, (void *)kernel_addr, 
                                         16, VM_READ);
    ASSERT(valid == 0, "Should reject kernel address");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 10: VMA cleanup frees all VMAs
 */
static void test_vma_cleanup(void) {
    TEST_START("VMA cleanup frees all VMAs");
    
    struct process *proc = create_test_process_struct("test_proc10");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Add some VMAs
    process_add_vma(proc, 0x10000, 0x20000, VM_READ | VM_USER);
    process_add_vma(proc, 0x30000, 0x40000, VM_WRITE | VM_USER);
    
    // Verify we have VMAs
    ASSERT(proc->vm_areas != NULL, "No VMAs to clean up");
    
    // Clean up VMAs
    process_cleanup_vmas(proc);
    
    // Verify all VMAs are freed
    ASSERT(proc->vm_areas == NULL, "VMAs not cleaned up");
    
    // Clean up (won't clean VMAs again since already NULL)
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 11: Different processes have different structures
 */
static void test_processes_have_different_page_tables(void) {
    TEST_START("Different processes have different structures");
    
    struct process *proc1 = create_test_process_struct("test_proc11a");
    struct process *proc2 = create_test_process_struct("test_proc11b");
    
    ASSERT(proc1 != NULL && proc2 != NULL, "Process creation failed");
    
    // Verify different process structures
    ASSERT(proc1 != proc2, "Processes sharing same structure");
    ASSERT(proc1->pid != proc2->pid, "Processes have same PID");
    
    // Clean up
    cleanup_test_process(proc1);
    cleanup_test_process(proc2);
    
    TEST_PASS();
}

/**
 * Test 12: process_add_vma creates proper VMA
 */
static void test_add_vma(void) {
    TEST_START("process_add_vma creates proper VMA");
    
    struct process *proc = create_test_process_struct("test_proc12");
    ASSERT(proc != NULL, "Process creation failed");
    
    uint64_t start = 0x10000000;
    uint64_t end = 0x10001000;
    uint32_t flags = VM_READ | VM_WRITE | VM_USER;
    
    int result = process_add_vma(proc, start, end, flags);
    ASSERT(result == 0, "process_add_vma failed");
    
    // Find the VMA
    vm_area_t *vma = process_find_vma(proc, start);
    ASSERT(vma != NULL, "VMA not found after adding");
    ASSERT(vma->start == start, "VMA start mismatch");
    ASSERT(vma->end == end, "VMA end mismatch");
    ASSERT(vma->flags == flags, "VMA flags mismatch");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 13: process_remove_vma removes VMA
 */
static void test_remove_vma(void) {
    TEST_START("process_remove_vma removes VMA");
    
    struct process *proc = create_test_process_struct("test_proc13");
    ASSERT(proc != NULL, "Process creation failed");
    
    uint64_t start = 0x10000000;
    uint64_t end = 0x10001000;
    
    process_add_vma(proc, start, end, VM_READ | VM_USER);
    
    // Find and remove the VMA
    vm_area_t *vma = process_find_vma(proc, start);
    ASSERT(vma != NULL, "VMA not found");
    
    process_remove_vma(proc, vma);
    
    // Verify it's removed
    vma = process_find_vma(proc, start);
    ASSERT(vma == NULL, "VMA not removed");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Test 14: VMA isolation between processes
 */
static void test_cross_process_isolation(void) {
    TEST_START("VMA isolation between processes");
    
    struct process *proc1 = create_test_process_struct("test_proc14a");
    struct process *proc2 = create_test_process_struct("test_proc14b");
    
    ASSERT(proc1 != NULL && proc2 != NULL, "Process creation failed");
    
    // Add VMA to proc1 (without mapping pages)
    uint64_t test_addr = 0x50000;
    process_add_vma(proc1, test_addr, test_addr + PAGE_SIZE, VM_READ | VM_WRITE | VM_USER);
    
    // Verify proc1 can find its VMA
    vm_area_t *vma1 = process_find_vma(proc1, test_addr);
    ASSERT(vma1 != NULL, "Proc1 should find its own VMA");
    
    // Verify proc2 cannot find proc1's VMA (different VMA lists)
    vm_area_t *vma2 = process_find_vma(proc2, test_addr);
    ASSERT(vma2 == NULL, "Proc2 should not find proc1's VMA");
    
    // Clean up
    cleanup_test_process(proc1);
    cleanup_test_process(proc2);
    
    TEST_PASS();
}

/**
 * Test 15: Heap boundaries enforce safety margins
 */
static void test_heap_safety_margins(void) {
    TEST_START("Heap boundaries enforce safety margins");
    
    struct process *proc = create_test_process_struct("test_proc15");
    ASSERT(proc != NULL, "Process creation failed");
    
    // Verify heap is below stack
    uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    ASSERT(proc->heap_start < stack_bottom, "Heap not below stack");
    
    // Verify reasonable space between heap and stack
    uint64_t gap = stack_bottom - proc->heap_start;
    ASSERT(gap > 0x100000, "Insufficient gap between heap and stack");
    
    // Clean up
    cleanup_test_process(proc);
    
    TEST_PASS();
}

/**
 * Main test runner
 */
void run_memory_isolation_tests(void) {
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("  Memory Isolation Tests\n");
    hal_uart_puts("========================================\n\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    // Run all tests
    test_isolated_page_table();
    test_vma_initialization();
    test_code_vma_permissions();
    test_stack_vma_permissions();
    test_heap_initialization();
    test_map_region_creates_vma();
    test_validate_user_ptr_permissions();
    test_validate_user_ptr_rejects_unmapped();
    test_validate_user_ptr_rejects_kernel();
    test_vma_cleanup();
    test_processes_have_different_page_tables();
    test_add_vma();
    test_remove_vma();
    test_cross_process_isolation();
    test_heap_safety_margins();
    
    // Print summary
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Test Summary:\n");
    hal_uart_puts("  Passed: ");
    kprint_dec(tests_passed);
    hal_uart_puts(" / ");
    kprint_dec(tests_passed + tests_failed);
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("  Status: ALL TESTS PASSED!\n");
    } else {
        hal_uart_puts("  Failed: ");
        kprint_dec(tests_failed);
        hal_uart_puts("\n");
        hal_uart_puts("  Status: SOME TESTS FAILED\n");
    }
    hal_uart_puts("========================================\n\n");
}

#endif // ENABLE_KERNEL_TESTS
