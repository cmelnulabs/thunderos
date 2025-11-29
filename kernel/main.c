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
#include "kernel/pipe.h"
#include "kernel/elf_loader.h"
#include "drivers/virtio_blk.h"
#include "drivers/virtio_gpu.h"
#include "drivers/framebuffer.h"
#include "drivers/fbconsole.h"
#include "drivers/vterm.h"
#include "drivers/font.h"
#include "fs/ext2.h"
#include "fs/vfs.h"

/* Constants */
#define TEST_ALLOC_SIZE         256
#define VIRTIO_PROBE_COUNT      8
#define VIRTIO_BASE_ADDRESS     0x10001000
#define VIRTIO_ADDRESS_STRIDE   0x1000

/* Linker symbols */
extern char _kernel_end[];

/* Global filesystem state */
ext2_fs_t g_root_ext2_fs;

#ifdef ENABLE_KERNEL_TESTS
extern void test_memory_management(void);
extern void test_elf_all(void);
extern void run_memory_isolation_tests(void);
#endif

/* Forward declarations for helper functions */
static void print_boot_banner(void);
static void init_interrupts(void);
static void init_memory(void);
static int init_block_device(void);
static int init_gpu_device(void);
static int init_filesystem(void);
static void launch_shell(void);
static void halt_cpu(void);

#ifdef ENABLE_KERNEL_TESTS
static void run_memory_tests(void);
#endif

/*
 * Print the boot banner and kernel load address.
 */
static void print_boot_banner(void) {
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("   ThunderOS - RISC-V AI OS\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("Kernel loaded at 0x");
    kprint_hex(KERNEL_LOAD_ADDRESS);
    hal_uart_puts("\n");
    hal_uart_puts("Initializing...\n\n");
}

/*
 * Initialize interrupt subsystem including PLIC, CLINT, and trap handling.
 */
static void init_interrupts(void) {
    interrupt_init();
    hal_uart_puts("[OK] Interrupt subsystem initialized\n");

    trap_init();
    hal_uart_puts("[OK] Trap handler initialized\n");

    interrupt_enable();
    hal_uart_puts("[OK] Interrupts enabled\n");

    hal_timer_init(TIMER_INTERVAL_US);
    hal_uart_puts("[OK] Timer interrupts enabled\n");
}

/*
 * Initialize physical memory manager, virtual memory, and DMA allocator.
 */
static void init_memory(void) {
    uintptr_t kernel_end_addr = (uintptr_t)_kernel_end;
    size_t free_memory_size = RAM_END_ADDRESS - kernel_end_addr;

    pmm_init(kernel_end_addr, free_memory_size);
    hal_uart_puts("[OK] Memory management initialized\n");

    paging_init(KERNEL_LOAD_ADDRESS, kernel_end_addr);
    hal_uart_puts("[OK] Virtual memory initialized\n");

    dma_init();
    hal_uart_puts("[OK] DMA allocator initialized\n");
}

#ifdef ENABLE_KERNEL_TESTS
/*
 * Run memory allocation tests and print statistics.
 */
static void run_memory_tests(void) {
    hal_uart_puts("\nTesting memory allocation:\n");

    hal_uart_puts("  Allocating page 1... ");
    uintptr_t page1 = pmm_alloc_page();
    if (page1) {
        hal_uart_puts("OK (addr: 0x");
        kprint_hex(page1);
        hal_uart_puts(")\n");
    } else {
        hal_uart_puts("FAILED\n");
    }

    hal_uart_puts("  Allocating page 2... ");
    uintptr_t page2 = pmm_alloc_page();
    if (page2) {
        hal_uart_puts("OK\n");
    } else {
        hal_uart_puts("FAILED\n");
    }

    hal_uart_puts("  Testing kmalloc(");
    kprint_dec(TEST_ALLOC_SIZE);
    hal_uart_puts(")... ");
    void *ptr = kmalloc(TEST_ALLOC_SIZE);
    if (ptr) {
        hal_uart_puts("OK\n");
    } else {
        hal_uart_puts("FAILED\n");
    }

    hal_uart_puts("  Freeing page 1... ");
    pmm_free_page(page1);
    hal_uart_puts("OK\n");

    hal_uart_puts("  Freeing kmalloc memory... ");
    kfree(ptr);
    hal_uart_puts("OK\n");

    size_t total_pages = 0;
    size_t free_pages = 0;
    pmm_get_stats(&total_pages, &free_pages);
    hal_uart_puts("\nMemory statistics:\n");
    hal_uart_puts("  Total pages: ");
    kprint_dec(total_pages);
    hal_uart_puts("\n  Free pages:  ");
    kprint_dec(free_pages);
    hal_uart_puts("\n");

    hal_uart_puts("\n[INFO] Running built-in kernel tests...\n");
    test_memory_management();
    test_elf_all();
    hal_uart_puts("[INFO] Built-in tests completed\n\n");
}
#endif

/*
 * Probe and initialize VirtIO block device.
 * Returns 0 on success, -1 if no device found.
 */
static int init_block_device(void) {
    hal_uart_puts("\n");

    for (int probe_index = 0; probe_index < VIRTIO_PROBE_COUNT; probe_index++) {
        uint64_t device_address = VIRTIO_BASE_ADDRESS + (probe_index * VIRTIO_ADDRESS_STRIDE);
        int irq_number = probe_index + 1;

        if (virtio_blk_init(device_address, irq_number) == 0) {
            hal_uart_puts("[OK] VirtIO block device initialized\n");
            return 0;
        }
    }

    hal_uart_puts("[WARN] No VirtIO block device found - running without filesystem\n");
    return -1;
}

/*
 * Probe and initialize VirtIO GPU device.
 * Returns 0 on success, -1 if no device found.
 */
static int init_gpu_device(void) {
    for (int probe_index = 0; probe_index < VIRTIO_PROBE_COUNT; probe_index++) {
        uint64_t device_address = VIRTIO_BASE_ADDRESS + (probe_index * VIRTIO_ADDRESS_STRIDE);
        int irq_number = probe_index + 1;

        if (virtio_gpu_init(device_address, irq_number) == 0) {
            /* Initialize framebuffer abstraction */
            if (fb_init() == 0) {
                hal_uart_puts("[OK] Framebuffer initialized\n");
                
                /* Initialize framebuffer console */
                if (fbcon_init() == 0) {
                    hal_uart_puts("[OK] Framebuffer console initialized\n");
                    
                    /* Initialize virtual terminals */
                    if (vterm_init() == 0) {
                        hal_uart_puts("[OK] Virtual terminals initialized (6 VTs, Alt+1-6 to switch)\n");
                        
                        /* Display boot message on VT1 */
                        vterm_set_colors(14, 0);  /* Bright cyan on black */
                        vterm_puts("========================================\n");
                        vterm_puts("    ThunderOS - RISC-V AI OS\n");
                        vterm_puts("    Virtual Terminal 1\n");
                        vterm_puts("========================================\n\n");
                        vterm_set_colors(7, 0);  /* Reset to default */
                        vterm_flush();
                    } else {
                        /* Fall back to simple framebuffer console */
                        fbcon_set_colors(FBCON_COLOR_BRIGHT_CYAN, FBCON_COLOR_BLACK);
                        fbcon_puts("========================================\n");
                        fbcon_puts("    ThunderOS - RISC-V AI OS\n");
                        fbcon_puts("    Graphical Console Active\n");
                        fbcon_puts("========================================\n\n");
                        fbcon_reset_colors();
                        fbcon_flush();
                    }
                }
            }
            return 0;
        }
    }

    hal_uart_puts("[INFO] No VirtIO GPU device found - console only mode\n");
    
    /* Initialize virtual terminals even without GPU (for console multiplexing) */
    if (vterm_init() == 0) {
        hal_uart_puts("[OK] Virtual terminals initialized (UART mode, ESC+1-6 to switch)\n");
    }
    
    return -1;
}

/*
 * Mount ext2 filesystem and register with VFS.
 * Returns 0 on success, -1 on failure.
 */
static int init_filesystem(void) {
    virtio_blk_device_t *block_device = virtio_blk_get_device();
    if (!block_device) {
        return -1;
    }

    if (ext2_mount(&g_root_ext2_fs, block_device) != 0) {
        hal_uart_puts("[FAIL] Failed to mount ext2 filesystem\n");
        return -1;
    }

    hal_uart_puts("[OK] ext2 filesystem mounted successfully!\n");
    hal_uart_puts("  Block size: ");
    hal_uart_put_uint32(g_root_ext2_fs.block_size);
    hal_uart_puts(" bytes\n");
    hal_uart_puts("  Total blocks: ");
    hal_uart_put_uint32(g_root_ext2_fs.superblock->s_blocks_count);
    hal_uart_puts("\n");
    hal_uart_puts("  Free blocks: ");
    hal_uart_put_uint32(g_root_ext2_fs.superblock->s_free_blocks_count);
    hal_uart_puts("\n");
    hal_uart_puts("  Inodes: ");
    hal_uart_put_uint32(g_root_ext2_fs.superblock->s_inodes_count);
    hal_uart_puts("\n");

    vfs_filesystem_t *vfs_fs = ext2_vfs_mount(&g_root_ext2_fs);
    if (!vfs_fs) {
        hal_uart_puts("[WARN] Failed to mount ext2 into VFS\n");
        return -1;
    }

    if (vfs_mount_root(vfs_fs) != 0) {
        hal_uart_puts("[WARN] Failed to set VFS root\n");
        return -1;
    }

    hal_uart_puts("[OK] VFS root filesystem mounted\n");
    return 0;
}

/*
 * Launch a shell on a specific virtual terminal.
 * Returns the PID of the launched shell, or -1 on error.
 */
static int launch_shell_on_vt(int vt_index) {
    int shell_pid = elf_load_exec("/bin/ush", NULL, 0);
    if (shell_pid < 0) {
        return -1;
    }
    
    /* Set the shell's controlling terminal */
    struct process *shell_proc = process_get(shell_pid);
    if (shell_proc) {
        process_set_tty(shell_proc, vt_index);
    }
    
    return shell_pid;
}

/*
 * Launch user-mode shells on all virtual terminals.
 */
static void launch_shell(void) {
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("  Starting User-Mode Shell\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("\n");

    /* For now, launch just one shell on VT1 */
    /* Multi-shell support requires per-terminal input queues */
    int shell_pid = launch_shell_on_vt(0);  /* VT1 */
    
    if (shell_pid < 0) {
        hal_uart_puts("[FAIL] Failed to launch user-mode shell\n");
        hal_uart_puts("Falling back to kernel shell...\n");
        shell_init();
        shell_run();
        return;
    }
    
    hal_uart_puts("[OK] User-mode shell launched (PID ");
    hal_uart_put_uint32(shell_pid);
    hal_uart_puts(")\n");
    
    if (vterm_available()) {
        hal_uart_puts("[INFO] Switch terminals with F1-F6 or ESC+1-6\n");
    }

    /* Wait for shell to exit */
    int exit_status = 0;
    sys_waitpid(shell_pid, &exit_status, 0);

    hal_uart_puts("[INFO] Shell exited with status ");
    hal_uart_put_uint32(exit_status);
    hal_uart_puts("\n");
}

/*
 * Halt the CPU in an infinite loop.
 */
static void halt_cpu(void) {
    hal_uart_puts("[INFO] System halted\n");
    __asm__ volatile("csrw sscratch, zero");

    while (1) {
        __asm__ volatile("wfi");
    }
}

/*
 * Main kernel entry point.
 */
void kernel_main(void) {
    hal_uart_init();
    hal_uart_puts("[OK] UART initialized\n");

    print_boot_banner();
    init_interrupts();
    init_memory();

#ifdef ENABLE_KERNEL_TESTS
    run_memory_tests();
#endif

    process_init();
    scheduler_init();

    pipe_init();
    hal_uart_puts("[OK] Pipe subsystem initialized\n");

    if (init_block_device() == 0) {
        init_filesystem();
    }

    /* Try to initialize GPU (optional - console works without it) */
    init_gpu_device();

#ifdef TEST_MODE
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("  Test Mode - Halting\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("[OK] All kernel tests completed\n");
#else
    launch_shell();
#endif

    halt_cpu();
}
