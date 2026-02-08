/*
 * HAL Timer Interface
 * 
 * This header defines the hardware abstraction layer for timer operations.
 * It provides a platform-independent interface for timer initialization,
 * interrupt handling, and tick counting.
 * 
 * Implementation notes:
 * - Each architecture must implement these functions
 * - Timer interrupts should increment an internal tick counter
 * - Timer interval is specified in microseconds
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

/* Timer frequency (QEMU virt machine) */
#ifndef TIMER_FREQ_HZ
#define TIMER_FREQ_HZ 10000000  /* 10 MHz */
#endif
#define TICKS_PER_MS  10000     /* Ticks per millisecond at 10MHz */

/**
 * Initialize the timer hardware and start periodic interrupts
 * 
 * This function should:
 * 1. Configure the hardware timer
 * 2. Set the initial interrupt interval
 * 3. Enable timer interrupts in the interrupt controller
 * 4. Enable global interrupts if needed
 * 
 * @param interval_us Timer interrupt interval in microseconds
 */
void hal_timer_init(unsigned long interval_us);

/**
 * Get the current tick count
 * 
 * Returns the number of timer interrupts that have occurred since
 * initialization. This counter is incremented by the timer interrupt
 * handler.
 * 
 * @return Current tick count
 */
unsigned long hal_timer_get_ticks(void);

/**
 * Set the next timer interrupt
 * 
 * Schedules the next timer interrupt to occur after the specified
 * number of microseconds from the current time.
 * 
 * @param interval_us Microseconds until next timer interrupt
 */
void hal_timer_set_next(unsigned long interval_us);

/**
 * Handle timer interrupt
 * 
 * This function is called by the interrupt handler when a timer
 * interrupt occurs. It should:
 * 1. Increment the tick counter
 * 2. Perform any timer-related bookkeeping
 * 3. Schedule the next timer interrupt
 * 
 * Note: This function is typically called from the trap handler
 */
void hal_timer_handle_interrupt(void);

#endif // HAL_TIMER_H
