/*
 * HAL UART Interface
 * 
 * Hardware abstraction for UART serial communication.
 * Each architecture must implement these functions.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

/**
 * Initialize UART hardware
 * 
 * Architecture-specific implementation:
 * - RISC-V: NS16550A UART on QEMU virt machine
 * - ARM64: PL011 UART (future)
 * - x86-64: 16550 UART (future)
 */
void hal_uart_init(void);

/**
 * Write a single character to UART
 * 
 * Blocks until the character is transmitted.
 * 
 * @param c Character to transmit
 */
void hal_uart_putc(char c);

/**
 * Write a null-terminated string to UART
 * 
 * Handles newline conversion internally (\n -> \r\n for terminal compatibility)
 * 
 * @param s Null-terminated string to transmit
 */
void hal_uart_puts(const char *s);

/**
 * Read a single character from UART
 * 
 * Blocks until a character is available.
 * 
 * @return Character received from UART
 */
char hal_uart_getc(void);

#endif /* HAL_UART_H */
