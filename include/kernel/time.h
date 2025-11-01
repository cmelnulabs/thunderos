/*
 * Kernel Time Utilities
 * 
 * Provides timing and delay functions using RISC-V timer.
 */

#ifndef KTIME_H
#define KTIME_H

#include <stdint.h>

/**
 * Read the current time value
 * 
 * @return Current time in timer ticks (10MHz on QEMU)
 */
static inline uint64_t ktime_read(void) {
    uint64_t time;
    __asm__ volatile("rdtime %0" : "=r"(time));
    return time;
}

/**
 * Delay for a specified number of microseconds
 * 
 * @param us Microseconds to delay
 */
void udelay(uint64_t us);

/**
 * Delay for a specified number of milliseconds
 * 
 * @param ms Milliseconds to delay
 */
void mdelay(uint64_t ms);

/**
 * Get elapsed time in microseconds between two time points
 * 
 * @param start Start time (from ktime_read)
 * @param end End time (from ktime_read)
 * @return Elapsed microseconds
 */
uint64_t ktime_elapsed_us(uint64_t start, uint64_t end);

#endif // KTIME_H
