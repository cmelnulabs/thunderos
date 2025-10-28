/*
 * ThunderOS - Main kernel entry point
 */

#include "hal/hal_uart.h"
#include "trap.h"
#include "clint.h"

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
    clint_init();
    hal_uart_puts("[OK] Timer interrupts enabled\n");
    
    hal_uart_puts("[  ] Memory management: TODO\n");
    hal_uart_puts("[  ] Process scheduler: TODO\n");
    hal_uart_puts("[  ] AI accelerators: TODO\n");
    
    hal_uart_puts("\nThunderOS kernel idle. Waiting for timer interrupts...\n");
    
    // Halt CPU (will wake on interrupts)
    while (1) {
        __asm__ volatile("wfi");
    }
}
