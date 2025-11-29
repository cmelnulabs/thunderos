/*
 * HAL UART Interface
 * 
 * Hardware abstraction for UART serial communication.
 * Each architecture must implement these functions.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

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
 * Write a buffer of bytes to UART
 * 
 * Writes multiple bytes efficiently without newline conversion.
 * 
 * @param buffer Buffer to transmit
 * @param count Number of bytes to write
 * @return Number of bytes written
 */
int hal_uart_write(const char *buffer, unsigned int count);

/**
 * Read a single character from UART
 * 
 * Blocks until a character is available.
 * 
 * @return Character received from UART
 */
char hal_uart_getc(void);

/**
 * Check if UART has data available to read
 * 
 * @return 1 if data available, 0 otherwise
 */
int hal_uart_data_available(void);

/**
 * Read a single character from UART (non-blocking)
 * 
 * Returns immediately if no data available.
 * 
 * @return Character received, or -1 if no data available
 */
int hal_uart_getc_nonblock(void);

/**
 * Write a 32-bit unsigned integer as decimal to UART
 * 
 * @param value Value to write
 */
void hal_uart_put_uint32(uint32_t value);

/**
 * Write a 32-bit unsigned integer as hexadecimal to UART
 * 
 * @param value Value to write
 */
void hal_uart_put_hex(uint32_t value);

#endif /* HAL_UART_H */
