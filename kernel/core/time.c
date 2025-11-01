/*
 * Kernel Time Utilities Implementation
 */

#include "kernel/time.h"

// Timer frequency on QEMU (10 MHz)
#define TIMER_FREQ 10000000UL

// Conversion constants
#define MICROSECONDS_PER_SECOND 1000000UL
#define MICROSECONDS_PER_MILLISECOND 1000UL

/**
 * Delay for a specified number of microseconds
 */
void udelay(uint64_t us) {
    if (us == 0) return;
    
    // Calculate target ticks
    uint64_t ticks = (TIMER_FREQ * us) / MICROSECONDS_PER_SECOND;
    
    uint64_t start = ktime_read();
    uint64_t target = start + ticks;
    
    // Wait until we reach the target time
    while (ktime_read() < target) {
        // Use WFI to reduce power consumption while waiting
        __asm__ volatile("wfi");
    }
}

/**
 * Delay for a specified number of milliseconds
 */
void mdelay(uint64_t ms) {
    udelay(ms * MICROSECONDS_PER_MILLISECOND);
}

/**
 * Get elapsed time in microseconds between two time points
 */
uint64_t ktime_elapsed_us(uint64_t start, uint64_t end) {
    uint64_t ticks = end - start;
    return (ticks * MICROSECONDS_PER_SECOND) / TIMER_FREQ;
}
