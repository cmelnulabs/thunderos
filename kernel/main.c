/*
 * ThunderOS - Main kernel entry point
 */

#include "hal/hal_uart.h"
#include "hal/hal_timer.h"
#include "trap.h"
#include "arch/interrupt.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "mm/paging.h"
#include "mm/dma.h"
#include "kernel/kstring.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/time.h"
#include "kernel/config.h"
#include "kernel/syscall.h"
#include "kernel/shell.h"
#include "drivers/virtio_blk.h"
#include "fs/ext2.h"
#include "fs/vfs.h"

// Test allocation size
#define TEST_ALLOC_SIZE 256             // Bytes for kmalloc test

// Linker symbols (defined in kernel.ld)
extern char _kernel_end[];

// External functions
ext2_fs_t g_test_ext2_fs;

#ifdef ENABLE_KERNEL_TESTS
// Built-in test functions (only compiled if ENABLE_KERNEL_TESTS is set)
extern void test_memory_management(void);
extern void test_elf_all(void);
#endif

// Demo process functions
void process_a(void *arg) {
    (void)arg;
    int count = 0;
    while (1) {
        hal_uart_puts("[Process A] Running... iteration ");
        kprint_dec(count++);
        hal_uart_puts("\n");
        
        // Yield to other processes (rely on preemptive scheduling)
        process_yield();
    }
}

void process_b(void *arg) {
    (void)arg;
    int count = 0;
    while (1) {
        hal_uart_puts("[Process B] Hello from B! count = ");
        kprint_dec(count++);
        hal_uart_puts("\n");
        
        // Yield to other processes (rely on preemptive scheduling)
        process_yield();
    }
}

void process_c(void *arg) {
    (void)arg;
    int count = 0;
    while (1) {
        hal_uart_puts("[Process C] Task C executing... #");
        kprint_dec(count++);
        hal_uart_puts("\n");
        
        // Yield to other processes (rely on preemptive scheduling)
        process_yield();
    }
}

void kernel_main(void) {
    // Initialize UART for serial output
    hal_uart_init();
    
    // Print welcome message
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("   ThunderOS - RISC-V AI OS\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("Kernel loaded at 0x");
    kprint_hex(KERNEL_LOAD_ADDRESS);
    hal_uart_puts("\n");
    hal_uart_puts("Initializing...\n\n");
    
    hal_uart_puts("[OK] UART initialized\n");
    
    // Initialize interrupt subsystem (PLIC + CLINT)
    interrupt_init();
    hal_uart_puts("[OK] Interrupt subsystem initialized\n");
    
    // Initialize trap handling
    trap_init();
    hal_uart_puts("[OK] Trap handler initialized\n");
    
    // Enable interrupts globally
    interrupt_enable();
    hal_uart_puts("[OK] Interrupts enabled\n");
    
    // Initialize timer interrupts
    hal_timer_init(TIMER_INTERVAL_US);
    hal_uart_puts("[OK] Timer interrupts enabled\n");
    
    // Initialize memory management
    // QEMU virt machine: 128MB RAM at 0x80000000 to 0x88000000
    // Our kernel ends at _kernel_end, so free memory starts there
    uintptr_t kernel_end = (uintptr_t)_kernel_end;
    uintptr_t mem_start = kernel_end;
    
    // Calculate free memory region
    size_t free_mem_size = RAM_END_ADDRESS - mem_start;
    
    pmm_init(mem_start, free_mem_size);
    hal_uart_puts("[OK] Memory management initialized\n");
    
    // Initialize virtual memory (paging)
    // Identity map the kernel region
    paging_init(KERNEL_LOAD_ADDRESS, kernel_end);
    hal_uart_puts("[OK] Virtual memory initialized\n");
    
    // Initialize DMA allocator
    dma_init();
    hal_uart_puts("[OK] DMA allocator initialized\n");
    
    // Test memory allocation
    hal_uart_puts("\nTesting memory allocation:\n");
    
    // Allocate a page
    hal_uart_puts("  Allocating page 1... ");
    uintptr_t page1 = pmm_alloc_page();
    if (page1) {
        hal_uart_puts("OK (addr: 0x");
        kprint_hex(page1);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("FAILED\n");
    }
    
    // Allocate another page
    hal_uart_puts("  Allocating page 2... ");
    uintptr_t page2 = pmm_alloc_page();
    if (page2) {
        hal_uart_puts("OK\n");
    } else {
        hal_uart_puts("FAILED\n");
    }
    
    // Test kmalloc
    hal_uart_puts("  Testing kmalloc(");
    kprint_dec(TEST_ALLOC_SIZE);
    hal_uart_puts(")... ");
    void *ptr = kmalloc(TEST_ALLOC_SIZE);
    if (ptr) {
        hal_uart_puts("OK\n");
    } else {
        hal_uart_puts("FAILED\n");
    }
    
    // Free page 1
    hal_uart_puts("  Freeing page 1... ");
    pmm_free_page(page1);
    hal_uart_puts("OK\n");
    
    // Free kmalloc'd memory
    hal_uart_puts("  Freeing kmalloc memory... ");
    kfree(ptr);
    hal_uart_puts("OK\n");
    
    // Print memory stats
    size_t total, free;
    pmm_get_stats(&total, &free);
    hal_uart_puts("\nMemory statistics:\n");
    hal_uart_puts("  Total pages: ");
    kprint_dec(total);
    hal_uart_puts("\n  Free pages:  ");
    kprint_dec(free);
    hal_uart_puts("\n");
    
#ifdef ENABLE_KERNEL_TESTS
    // Run built-in kernel tests
    hal_uart_puts("\n[INFO] Running built-in kernel tests...\n");
    test_memory_management();
    test_elf_all();
    hal_uart_puts("[INFO] Built-in tests completed\n\n");
#endif
    
    // Initialize process management
    process_init();
    
    // Initialize scheduler
    scheduler_init();
    
    // Skip demo processes - going straight to interactive shell
    /*
    // Create demo processes
    hal_uart_puts("\nCreating demo processes...\n");
    
    struct process *proc_a = process_create("proc_a", process_a, NULL);
    if (proc_a) {
        hal_uart_puts("[OK] Created Process A (PID ");
        kprint_dec(proc_a->pid);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("[FAIL] Failed to create Process A\n");
    }
    
    struct process *proc_b = process_create("proc_b", process_b, NULL);
    if (proc_b) {
        hal_uart_puts("[OK] Created Process B (PID ");
        kprint_dec(proc_b->pid);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("[FAIL] Failed to create Process B\n");
    }
    
    struct process *proc_c = process_create("proc_c", process_c, NULL);
    if (proc_c) {
        hal_uart_puts("[OK] Created Process C (PID ");
        kprint_dec(proc_c->pid);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("[FAIL] Failed to create Process C\n");
    }
    
    // Dump process table
    process_dump();
    
    // ====================================================================
    // Test User Mode Execution and Exception Handling
    // ====================================================================
    
    // Include the exception test program (assembly version)
    extern uint8_t user_exception_start[];
    
    // Calculate the size (hardcoded for now - 24 bytes for the assembly code)
    size_t code_size = 24;
    void *user_code = user_exception_start;
    
    struct process *user_proc = process_create_user("user_exception_test", user_code, code_size);
    if (user_proc) {
        hal_uart_puts("[OK] Created user process (PID ");
        kprint_dec(user_proc->pid);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("[FAIL] Failed to create user process\n");
    }
    
    // Dump updated process table
    process_dump();
    
    hal_uart_puts("\nThunderOS: Multitasking enabled!\n");
    hal_uart_puts("Processes will start running on next timer interrupt...\n\n");
    */
    
    // Initialize and run the interactive shell
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("  Starting Interactive Shell\n");
    hal_uart_puts("=================================\n");
    
    // Test VirtIO block device (simple test before shell)
    hal_uart_puts("\n[TEST] VirtIO Block Device\n");
    
    // Initialize VirtIO block device
    uint64_t virtio_addrs[] = {
        0x10001000, 0x10002000, 0x10003000, 0x10004000,
        0x10005000, 0x10006000, 0x10007000, 0x10008000
    };
    
    int result = -1;
    for (int i = 0; i < 8; i++) {
        result = virtio_blk_init(virtio_addrs[i], 1 + i);
        if (result == 0) {
            hal_uart_puts("[OK] VirtIO block device initialized\n");
            break;
        }
    }
    
    if (result != 0) {
        hal_uart_puts("[WARN] No VirtIO block device found - running without filesystem\n");
    } else {
        // Try mounting ext2 filesystem
        hal_uart_puts("[TEST] Mounting ext2 filesystem...\n");
        virtio_blk_device_t *blk_dev = virtio_blk_get_device();
        if (blk_dev) {
            int ret = ext2_mount(&g_test_ext2_fs, blk_dev);
            if (ret == 0) {
                hal_uart_puts("[OK] ext2 filesystem mounted successfully!\n");
                hal_uart_puts("  Block size: ");
                hal_uart_put_uint32(g_test_ext2_fs.block_size);
                hal_uart_puts(" bytes\n");
                hal_uart_puts("  Total blocks: ");
                hal_uart_put_uint32(g_test_ext2_fs.superblock->s_blocks_count);
                hal_uart_puts("\n");
                hal_uart_puts("  Free blocks: ");
                hal_uart_put_uint32(g_test_ext2_fs.superblock->s_free_blocks_count);
                hal_uart_puts("\n");
                hal_uart_puts("  Inodes: ");
                hal_uart_put_uint32(g_test_ext2_fs.superblock->s_inodes_count);
                hal_uart_puts("\n");
                
                // Mount into VFS
                vfs_filesystem_t *vfs_fs = ext2_vfs_mount(&g_test_ext2_fs);
                if (vfs_fs) {
                    ret = vfs_mount_root(vfs_fs);
                    if (ret == 0) {
                        hal_uart_puts("[OK] VFS root filesystem mounted\n");
                    } else {
                        hal_uart_puts("[WARN] Failed to set VFS root\n");
                    }
                } else {
                    hal_uart_puts("[WARN] Failed to mount ext2 into VFS\n");
                }
            } else {
                hal_uart_puts("[FAIL] Failed to mount ext2 filesystem\n");
            }
        }
    }
    
    hal_uart_puts("\n");
    shell_init();
    shell_run();
    
    // Halt CPU (should never reach here)
    while (1) {
        __asm__ volatile("wfi");
    }
}
