/*
 * KUnit test runner implementation
 */

#include "kunit.h"
#include "hal/hal_uart.h"

// Helper to print integers
static void print_int(int val) {
    if (val == 0) {
        hal_uart_putc('0');
        return;
    }
    
    if (val < 0) {
        hal_uart_putc('-');
        val = -val;
    }
    
    char buf[12];
    int i = 0;
    
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    
    while (i > 0) {
        hal_uart_putc(buf[--i]);
    }
}

// Run all test cases
int kunit_run_tests(struct kunit_test *test_cases, int num_tests) {
    int passed = 0;
    int failed = 0;
    
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  KUnit Test Suite - ThunderOS\n");
    hal_uart_puts("========================================\n\n");
    
    // Run each test
    for (int i = 0; i < num_tests; i++) {
        struct kunit_test *test = &test_cases[i];
        
        // Reset test status
        test->status = TEST_SUCCESS;
        test->failure_msg = NULL;
        test->line = 0;
        
        // Run the test
        hal_uart_puts("[ RUN      ] ");
        hal_uart_puts(test->name);
        hal_uart_puts("\n");
        
        test->run(test);
        
        // Check result
        if (test->status == TEST_SUCCESS) {
            hal_uart_puts("[       OK ] ");
            hal_uart_puts(test->name);
            hal_uart_puts("\n");
            passed++;
        } else {
            hal_uart_puts("[  FAILED  ] ");
            hal_uart_puts(test->name);
            hal_uart_puts("\n");
            hal_uart_puts("             ");
            hal_uart_puts(test->failure_msg);
            hal_uart_puts(" at line ");
            print_int(test->line);
            hal_uart_puts("\n");
            failed++;
        }
    }
    
    // Print summary
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  Test Summary\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("Total:  ");
    print_int(num_tests);
    hal_uart_puts("\n");
    hal_uart_puts("Passed: ");
    print_int(passed);
    hal_uart_puts("\n");
    hal_uart_puts("Failed: ");
    print_int(failed);
    hal_uart_puts("\n");
    
    if (failed == 0) {
        hal_uart_puts("\nALL TESTS PASSED\n");
    } else {
        hal_uart_puts("\nSOME TESTS FAILED\n");
    }
    
    hal_uart_puts("========================================\n\n");
    
    return failed;
}
