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

/**
 * Copy a string
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @return Destination buffer
 */
char *kstrcpy(char *dest, const char *src);

/**
 * Copy a string with length limit
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of characters to copy
 * @return Destination buffer
 */
char *kstrncpy(char *dest, const char *src, size_t n);

/**
 * Get string length
 * 
 * @param str String
 * @return Length of string
 */
size_t kstrlen(const char *str);

/**
 * Set memory to a value
 * 
 * @param s Memory area
 * @param c Value to set
 * @param n Number of bytes
 * @return Memory area
 */
void *kmemset(void *s, int c, size_t n);

/**
 * Copy memory
 * 
 * @param dest Destination
 * @param src Source
 * @param n Number of bytes
 * @return Destination
 */
void *kmemcpy(void *dest, const void *src, size_t n);

#endif // KSTRING_H
