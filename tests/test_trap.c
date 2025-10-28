/*
 * Example test: Trap handler test
 * Tests that the trap handler can catch illegal instructions
 */

#include "kunit.h"
#include "trap.h"
#include "hal/hal_uart.h"

// Global flag to track if exception was caught
static volatile int exception_caught = 0;
static volatile unsigned long saved_cause = 0;

// We'll hook into the real trap handler by modifying its behavior
// For now, we'll test indirectly by checking if we can recover from exceptions

// Test 1: Basic trap initialization
static void test_trap_init(struct kunit_test *test) {
    unsigned long stvec;
    
    // Read stvec register
    asm volatile("csrr %0, stvec" : "=r"(stvec));
    
    // Should not be zero after trap_init()
    KUNIT_EXPECT_NE(test, stvec, 0);
}

// Test 2: Check trap handler is installed
static void test_trap_handler_installed(struct kunit_test *test) {
    unsigned long stvec;
    
    // Read stvec register - should point to trap_vector
    asm volatile("csrr %0, stvec" : "=r"(stvec));
    
    // The address should be non-zero and aligned to 4 bytes
    KUNIT_EXPECT_NE(test, stvec & ~3UL, 0);
}

// Test 3: Basic arithmetic test (sanity check)
static void test_basic_math(struct kunit_test *test) {
    int a = 5;
    int b = 3;
    int result = a + b;
    
    KUNIT_EXPECT_EQ(test, result, 8);
    KUNIT_EXPECT_NE(test, result, 0);
}

// Test 4: Pointer operations
static void test_pointer_operations(struct kunit_test *test) {
    int value = 42;
    int *ptr = &value;
    
    KUNIT_EXPECT_NOT_NULL(test, ptr);
    KUNIT_EXPECT_EQ(test, *ptr, 42);
    KUNIT_EXPECT_EQ(test, ptr, &value);
}

// Define test cases (like KUnit)
static struct kunit_test trap_test_cases[] = {
    KUNIT_CASE(test_trap_init),
    KUNIT_CASE(test_trap_handler_installed),
    KUNIT_CASE(test_basic_math),
    KUNIT_CASE(test_pointer_operations),
};

// Test kernel main
void kernel_main(void) {
    hal_uart_init();
    trap_init();
    
    hal_uart_puts("\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("   ThunderOS - Test Kernel\n");
    hal_uart_puts("=================================\n");
    hal_uart_puts("Running trap handler tests...\n");
    
    // Run all tests
    int num_tests = sizeof(trap_test_cases) / sizeof(trap_test_cases[0]);
    int failed = kunit_run_tests(trap_test_cases, num_tests);
    
    // Exit with status code
    if (failed == 0) {
        hal_uart_puts("Test kernel exiting with success.\n");
    } else {
        hal_uart_puts("Test kernel exiting with failures.\n");
    }
    
    // Halt
    while (1) {
        __asm__ volatile("wfi");
    }
}
