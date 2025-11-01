/*
 * Kernel Panic Handler Implementation
 * 
 * Handles fatal kernel errors by halting the system safely.
 */

#include "kernel/panic.h"
#include "kernel/kstring.h"
#include "hal/hal_uart.h"
#include "arch/interrupt.h"

/**
 * Kernel panic - fatal error handler
 * 
 * This function never returns. It disables interrupts, prints
 * the error message, and halts the system.
 */
void kernel_panic(const char *message) {
    // Disable all interrupts immediately
    interrupt_disable();
    
    // Print panic banner
    hal_uart_puts("\n");
    hal_uart_puts("================================================================================\n");
    hal_uart_puts("                             KERNEL PANIC                                       \n");
    hal_uart_puts("================================================================================\n");
    hal_uart_puts("\n");
    
    // Print the panic message
    hal_uart_puts("Panic: ");
    hal_uart_puts(message);
    hal_uart_puts("\n\n");
    
    // Print system state information
    hal_uart_puts("System halted. The kernel encountered a fatal error and cannot continue.\n");
    hal_uart_puts("\n");
    
    // Print register state (if possible)
    hal_uart_puts("Register dump:\n");
    
    // Read some key RISC-V CSRs for debugging
    unsigned long sstatus, sepc, scause, stval;
    
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    asm volatile("csrr %0, sepc" : "=r"(sepc));
    asm volatile("csrr %0, scause" : "=r"(scause));
    asm volatile("csrr %0, stval" : "=r"(stval));
    
    hal_uart_puts("  sstatus = 0x");
    kprint_hex(sstatus);
    hal_uart_puts("\n");
    
    hal_uart_puts("  sepc    = 0x");
    kprint_hex(sepc);
    hal_uart_puts("\n");
    
    hal_uart_puts("  scause  = 0x");
    kprint_hex(scause);
    hal_uart_puts("\n");
    
    hal_uart_puts("  stval   = 0x");
    kprint_hex(stval);
    hal_uart_puts("\n");
    
    hal_uart_puts("\n");
    hal_uart_puts("================================================================================\n");
    
    // Halt the system - infinite loop with WFI
    hal_uart_puts("System halted. Press Ctrl+A then X to exit QEMU.\n");
    
    while (1) {
        asm volatile("wfi");  // Wait for interrupt (saves power)
    }
    
    // This should never be reached, but satisfies noreturn attribute
    __builtin_unreachable();
}
