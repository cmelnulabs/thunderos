/*
 * Kernel String Utilities
 * 
 * Helper functions for string manipulation and formatting.
 */

#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>

/**
 * Print a decimal number to UART
 * 
 * @param n Number to print
 */
void kprint_dec(size_t n);

/**
 * Print a hexadecimal number to UART
 * 
 * @param val Number to print in hex format (with 0x prefix)
 */
void kprint_hex(uintptr_t val);

#endif // KSTRING_H
