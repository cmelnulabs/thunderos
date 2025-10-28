/*
 * ThunderOS - Main kernel entry point
 */

#include "hal/hal_uart.h"
#include "hal/hal_timer.h"
#include "trap.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "kernel/kstring.h"

// Timer interval: 1 second = 1,000,000 microseconds
#define TIMER_INTERVAL_US 1000000

// Linker symbols (defined in kernel.ld)
extern char _kernel_end[];

void kernel_main(void) {
    // Initialize UART for serial output
    hal_uart_init();
    
    // Print welcome message
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("   ThunderOS - RISC-V AI OS\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("Kernel loaded at 0x80200000\n");
    hal_uart_puts("Initializing...\n\n");
    
    hal_uart_puts("[OK] UART initialized\n");
    
    // Initialize trap handling
    trap_init();
    hal_uart_puts("[OK] Trap handler initialized\n");
    
    // Initialize timer interrupts
    hal_timer_init(TIMER_INTERVAL_US);
    hal_uart_puts("[OK] Timer interrupts enabled\n");
    
    // Initialize memory management
    // QEMU virt machine has 128MB RAM at 0x80000000
    // Our kernel ends at _kernel_end, so free memory starts there
    uintptr_t kernel_end = (uintptr_t)_kernel_end;
    uintptr_t mem_start = kernel_end;
    
    // Total RAM: 128MB, starts at 0x80000000, ends at 0x88000000
    // Free memory = from kernel_end to end of RAM
    uintptr_t ram_end = 0x88000000;
    size_t free_mem_size = ram_end - mem_start;
    
    pmm_init(mem_start, free_mem_size);
    hal_uart_puts("[OK] Memory management initialized\n");
    
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
    hal_uart_puts("  Testing kmalloc(256)... ");
    void *ptr = kmalloc(256);
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
    
    hal_uart_puts("\n[  ] Process scheduler: TODO\n");
    hal_uart_puts("[  ] AI accelerators: TODO\n");
    
    hal_uart_puts("\nThunderOS kernel idle. Waiting for timer interrupts...\n");
    
    // Halt CPU (will wake on interrupts)
    while (1) {
        __asm__ volatile("wfi");
    }
}
