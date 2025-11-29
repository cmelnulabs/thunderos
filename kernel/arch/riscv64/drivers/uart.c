/*
 * RISC-V UART Driver (NS16550A compatible)
 * 
 * Implementation of HAL UART interface for RISC-V architecture.
 * Used in QEMU virt machine.
 */

#include "hal/hal_uart.h"

// UART0 base address on QEMU virt machine
#define UART0_BASE 0x10000000

// UART registers (NS16550A)
#define UART_RBR (UART0_BASE + 0)  // Receiver Buffer Register (read)
#define UART_THR (UART0_BASE + 0)  // Transmitter Holding Register (write)
#define UART_LSR (UART0_BASE + 5)  // Line Status Register

// Line Status Register bits
#define LSR_DATA_READY (1 << 0)    // Data available to read
#define LSR_TX_IDLE    (1 << 5)    // Transmitter idle (can write)

// Helper to write to UART register
static inline void uart_write_reg(unsigned long addr, unsigned char val) {
    *(volatile unsigned char *)addr = val;
}

// Helper to read from UART register
static inline unsigned char uart_read_reg(unsigned long addr) {
    return *(volatile unsigned char *)addr;
}

/*
 * HAL Implementation
 */

void hal_uart_init(void) {
    // QEMU's UART is already initialized by OpenSBI firmware
    // No additional setup needed for basic operation
    //
    // On real hardware, you would configure:
    // - Baud rate (e.g., 115200)
    // - Data bits (8)
    // - Stop bits (1)
    // - Parity (none)
}

void hal_uart_putc(char c) {
    // Wait until transmitter holding register is empty
    while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
        ;
    
    // Write character to transmitter
    uart_write_reg(UART_THR, c);
}

void hal_uart_puts(const char *s) {
    while (*s) {
        // Convert Unix newline to DOS newline for terminal compatibility
        if (*s == '\n') {
            hal_uart_putc('\r');
        }
        hal_uart_putc(*s++);
    }
}

int hal_uart_write(const char *buffer, unsigned int count) {
    unsigned int bytes_written = 0;
    
    for (unsigned int i = 0; i < count; i++) {
        // Wait until transmitter holding register is empty
        while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
            ;
        
        // Write character to transmitter
        uart_write_reg(UART_THR, buffer[i]);
        bytes_written++;
    }
    
    return bytes_written;
}

char hal_uart_getc(void) {
    // Wait for data to be available
    while ((uart_read_reg(UART_LSR) & LSR_DATA_READY) == 0)
        ;
    
    // Read character from receiver buffer
    return uart_read_reg(UART_RBR);
}

int hal_uart_data_available(void) {
    return (uart_read_reg(UART_LSR) & LSR_DATA_READY) != 0;
}

int hal_uart_getc_nonblock(void) {
    if ((uart_read_reg(UART_LSR) & LSR_DATA_READY) == 0) {
        return -1;  // No data available
    }
    return (unsigned char)uart_read_reg(UART_RBR);
}

void hal_uart_put_uint32(uint32_t value) {
    // Convert to decimal string
    char buffer[11];  // Max 10 digits + null terminator
    int i = 0;
    
    if (value == 0) {
        hal_uart_putc('0');
        return;
    }
    
    // Extract digits in reverse order
    uint32_t temp = value;
    while (temp > 0) {
        buffer[i++] = '0' + (temp % 10);
        temp /= 10;
    }
    
    // Print digits in correct order
    for (int j = i - 1; j >= 0; j--) {
        hal_uart_putc(buffer[j]);
    }
}

void hal_uart_put_hex(uint32_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    
    hal_uart_puts("0x");
    
    // Print 8 hex digits
    for (int i = 7; i >= 0; i--) {
        uint32_t nibble = (value >> (i * 4)) & 0xF;
        hal_uart_putc(hex_chars[nibble]);
    }
}
