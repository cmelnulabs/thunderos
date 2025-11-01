/*
 * Kernel Time Utilities Implementation
 */

#include "kernel/time.h"

// Timer frequency on QEMU (10 MHz)
#define TIMER_FREQ 10000000UL

/**
 * Delay for a specified number of microseconds
 */
void udelay(uint64_t us) {
    if (us == 0) return;
    
    // Calculate target ticks
    uint64_t ticks = (TIMER_FREQ * us) / 1000000;
    
    uint64_t start = ktime_read();
    uint64_t target = start + ticks;
    
    // Wait until we reach the target time
    while (ktime_read() < target) {
        // Busy wait
    }
}

/**
 * Delay for a specified number of milliseconds
 */
void mdelay(uint64_t ms) {
    udelay(ms * 1000);
}

/**
 * Get elapsed time in microseconds between two time points
 */
uint64_t ktime_elapsed_us(uint64_t start, uint64_t end) {
    uint64_t ticks = end - start;
    return (ticks * 1000000) / TIMER_FREQ;
}
