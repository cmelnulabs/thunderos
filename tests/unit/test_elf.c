/*
 * ELF Loader Tests
 * 
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/elf_loader.h"
#include "fs/vfs.h"
#include "hal/hal_uart.h"
#include <stdint.h>

// Helper to print numbers
static void print_num(uint32_t num) {
    if (num == 0) {
        hal_uart_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i > 0) {
        hal_uart_putc(buf[--i]);
    }
}

void test_elf_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       ELF Loader Tests\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("\n");
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    // Test 1: Try to load non-existent file
    hal_uart_puts("[TEST] Load non-existent file\n");
    int result = elf_load_exec("/nonexistent", NULL, 0);
    if (result < 0) {
        hal_uart_puts("  [PASS] Failed as expected\n");
        tests_passed++;
    } else {
        hal_uart_puts("  [FAIL] Should have failed\n");
        tests_failed++;
    }
    
    // Test 2: Try to load invalid ELF (text file)
    hal_uart_puts("\n[TEST] Load invalid ELF file\n");
    result = elf_load_exec("/test.txt", NULL, 0);
    if (result < 0) {
        hal_uart_puts("  [PASS] Failed as expected\n");
        tests_passed++;
    } else {
        hal_uart_puts("  [FAIL] Should have failed\n");
        tests_failed++;
    }
    
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("Tests passed: ");
    print_num(tests_passed);
    hal_uart_puts(", Tests failed: ");
    print_num(tests_failed);
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("*** ALL TESTS PASSED ***\n");
    } else {
        hal_uart_puts("*** SOME TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n");
}

#endif // ENABLE_KERNEL_TESTS
