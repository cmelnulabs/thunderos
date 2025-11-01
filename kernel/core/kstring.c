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

/**
 * Copy a string
 */
char *kstrcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * Copy a string with length limit
 */
char *kstrncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/**
 * Get string length
 */
size_t kstrlen(const char *str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

/**
 * Set memory to a value
 */
void *kmemset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

/**
 * Copy memory
 */
void *kmemcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
