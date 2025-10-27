/*
 * Timer interrupt tests
 * Validates CLINT timer functionality
 */

#include "kunit.h"
#include "trap.h"
#include "uart.h"
#include "clint.h"

// Test 1: Verify timer interrupts are enabled in sie
static void test_timer_interrupts_enabled(struct kunit_test *test) {
    unsigned long sie;
    
    // Read sie register
    asm volatile("csrr %0, sie" : "=r"(sie));
    
    // Bit 5 should be set (STIE - Supervisor Timer Interrupt Enable)
    KUNIT_EXPECT_TRUE(test, sie & (1 << 5));
}

// Test 2: Verify global interrupts are enabled in sstatus
static void test_global_interrupts_enabled(struct kunit_test *test) {
    unsigned long sstatus;
    
    // Read sstatus register
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    
    // Bit 1 should be set (SIE - Supervisor Interrupt Enable)
    KUNIT_EXPECT_TRUE(test, sstatus & (1 << 1));
}

// Test 3: Verify initial tick count is zero
static void test_initial_ticks_zero(struct kunit_test *test) {
    unsigned long ticks = clint_get_ticks();
    KUNIT_EXPECT_EQ(test, ticks, 0);
}

// Test 4: Wait for timer interrupt and verify tick increments
static void test_timer_tick_increments(struct kunit_test *test) {
    unsigned long initial_ticks = clint_get_ticks();
    
    uart_puts("Waiting for timer interrupt (1 second)...\n");
    
    // Wait for tick to increment (with timeout)
    int timeout = 0;
    while (clint_get_ticks() == initial_ticks && timeout < 2000000) {
        asm volatile("wfi");  // Wait for interrupt
        timeout++;
    }
    
    unsigned long final_ticks = clint_get_ticks();
    
    // Tick should have incremented
    KUNIT_EXPECT_TRUE(test, final_ticks > initial_ticks);
}

// Test 5: Verify multiple ticks
static void test_multiple_ticks(struct kunit_test *test) {
    unsigned long start_ticks = clint_get_ticks();
    
    uart_puts("Waiting for 2 timer interrupts...\n");
    
    // Wait for at least 2 ticks
    int timeout = 0;
    while ((clint_get_ticks() - start_ticks) < 2 && timeout < 5000000) {
        asm volatile("wfi");
        timeout++;
    }
    
    unsigned long final_ticks = clint_get_ticks();
    
    // Should have at least 2 more ticks
    KUNIT_EXPECT_TRUE(test, (final_ticks - start_ticks) >= 2);
}

// Test 6: Verify rdtime instruction works
static void test_rdtime_works(struct kunit_test *test) {
    unsigned long time1, time2;
    
    asm volatile("rdtime %0" : "=r"(time1));
    
    // Do some work
    for (volatile int i = 0; i < 1000; i++);
    
    asm volatile("rdtime %0" : "=r"(time2));
    
    // Time should have advanced
    KUNIT_EXPECT_TRUE(test, time2 > time1);
}

// Define test cases
static struct kunit_test timer_test_cases[] = {
    KUNIT_CASE(test_timer_interrupts_enabled),
    KUNIT_CASE(test_global_interrupts_enabled),
    KUNIT_CASE(test_initial_ticks_zero),
    KUNIT_CASE(test_rdtime_works),
    KUNIT_CASE(test_timer_tick_increments),
    KUNIT_CASE(test_multiple_ticks),
};

// Test kernel main
void kernel_main(void) {
    uart_init();
    trap_init();
    clint_init();
    
    uart_puts("\n");
    uart_puts("=================================\n");
    uart_puts("   ThunderOS - Test Kernel\n");
    uart_puts("   Timer Interrupt Tests\n");
    uart_puts("=================================\n");
    
    // Run all tests
    int num_tests = sizeof(timer_test_cases) / sizeof(timer_test_cases[0]);
    int failed = kunit_run_tests(timer_test_cases, num_tests);
    
    // Exit with status code
    if (failed == 0) {
        uart_puts("All timer tests passed!\n");
    } else {
        uart_puts("Some timer tests failed!\n");
    }
    
    // Halt
    while (1) {
        __asm__ volatile("wfi");
    }
}
