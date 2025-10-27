/*
 * ThunderOS - Main kernel entry point
 */

#include "uart.h"
#include "trap.h"

void kernel_main(void) {
    // Initialize UART for serial output
    uart_init();
    
    // Print welcome message
    uart_puts("\n");
    uart_puts("=================================\n");
    uart_puts("   ThunderOS - RISC-V AI OS\n");
    uart_puts("=================================\n");
    uart_puts("Kernel loaded at 0x80200000\n");
    uart_puts("Initializing...\n\n");
    
    uart_puts("[OK] UART initialized\n");
    
    // Initialize trap handling
    trap_init();
    uart_puts("[OK] Trap handler initialized\n");
    
    uart_puts("[  ] Memory management: TODO\n");
    uart_puts("[  ] Process scheduler: TODO\n");
    uart_puts("[  ] AI accelerators: TODO\n");
    
    uart_puts("\nThunderOS kernel idle.\n");
    
    // Halt CPU
    while (1) {
        __asm__ volatile("wfi");
    }
}
