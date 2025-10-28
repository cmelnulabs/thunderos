/*
 * Kernel String Utilities Implementation
 */

#include "kernel/kstring.h"
#include "hal/hal_uart.h"

/**
 * Print a decimal number to UART
 */
void kprint_dec(size_t n) {
    if (n == 0) {
        hal_uart_putc('0');
        return;
    }
    
    char buf[20];
    int i = 0;
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    // Print in reverse order
    while (i > 0) {
        hal_uart_putc(buf[--i]);
    }
}

/**
 * Print a hexadecimal number to UART
 */
void kprint_hex(uintptr_t val) {
    char hex[19]; // "0x" + 16 hex digits + null
    hex[0] = '0';
    hex[1] = 'x';
    
    for (int i = 15; i >= 0; i--) {
        int digit = (val >> (i * 4)) & 0xF;
        hex[17 - i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
    }
    hex[18] = '\0';
    
    hal_uart_puts(hex);
}
