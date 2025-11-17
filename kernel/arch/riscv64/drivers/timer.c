/*
 * RISC-V Timer Driver (HAL Implementation)
 * 
 * This implements the HAL timer interface for RISC-V architecture.
 * Uses SBI (Supervisor Binary Interface) for timer control on QEMU.
 * 
 * Hardware: CLINT (Core Local Interruptor)
 * Method: SBI ecall for setting timer comparator
 */

#include "hal/hal_timer.h"
#include "hal/hal_uart.h"

// Timer frequency on QEMU (10 MHz)
#define TIMER_FREQ 10000000UL

// Global tick counter
static volatile unsigned long ticks = 0;

// Configured timer interval (in microseconds)
static unsigned long timer_interval_us = 0;

// SBI call numbers
#define SBI_SET_TIMER 0

/**
 * Make SBI call to set timer
 * 
 * The SBI (Supervisor Binary Interface) provides a standard way for
 * the supervisor mode (where our kernel runs) to request services
 * from machine mode (where the firmware/bootloader runs).
 */
static inline void sbi_set_timer(unsigned long stime_value) {
    register unsigned long a0 asm("a0") = stime_value;
    register unsigned long a7 asm("a7") = SBI_SET_TIMER;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

/**
 * Read the time CSR
 * 
 * RISC-V provides a cycle-accurate timer through the 'time' CSR.
 * This is a 64-bit counter that increments at a constant frequency.
 */
static inline unsigned long read_time(void) {
    unsigned long time;
    asm volatile("rdtime %0" : "=r"(time));
    return time;
}

/**
 * Check if SSTC extension is available
 * 
 * SSTC (Supervisor-mode Timer Compare) allows S-mode to directly
 * program timer interrupts via the stimecmp CSR, without SBI calls.
 * 
 * Since we're running our own M-mode code (not OpenSBI), SSTC is
 * already enabled in start.c via menvcfg.STCE
 */
static inline int has_sstc(void) {
    // SSTC is enabled by our M-mode start.c code
    return 1;  // Always use SSTC (no SBI available)
}

/**
 * Set timer using SSTC extension (direct stimecmp write)
 * 
 * Writing to stimecmp (0x14D) clears the pending interrupt automatically
 * and sets the next timer compare value.
 */
static inline void sstc_set_timer(unsigned long stime_value) {
    // Write to stimecmp CSR - this clears pending interrupt and sets next compare value
    asm volatile("csrw 0x14D, %0" :: "r"(stime_value));
}

/**
 * Initialize timer hardware and start periodic interrupts
 */
void hal_timer_init(unsigned long interval_us) {
    // Save the interval for later use
    timer_interval_us = interval_us;
    
    // Calculate ticks for the desired interval
    unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;
    
    // Set first timer interrupt
    unsigned long current_time = read_time();
    unsigned long next_time = current_time + interval_ticks;
    
    hal_uart_puts("[TIMER] Initialized\n");
    
    // Try SSTC first (newer, more efficient)
    if (has_sstc()) {
        hal_uart_puts("[TIMER] Using SSTC extension for timer\n");
        sstc_set_timer(next_time);
    } else {
        // This path won't be reached since we always have SSTC
        hal_uart_puts("[TIMER] Using SBI for timer\n");
        sbi_set_timer(next_time);
    }
    
    // Enable timer interrupts in sie (supervisor interrupt enable)
    // NOTE: M-mode initialized sie with timer interrupts DISABLED
    // We enable them here once we've programmed stimecmp
    unsigned long sie;
    asm volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1 << 5);  // STIE - Supervisor Timer Interrupt Enable
    asm volatile("csrw sie, %0" :: "r"(sie));
    
    // NOTE: Do NOT enable global interrupts here (sstatus.SIE)
    // That's already done by interrupt_enable() in main.c
    // We just needed to enable timer-specific interrupts in sie
    
    // Print initialization message
    hal_uart_puts("Timer initialized (interval: ");
    if (interval_us >= 1000000) {
        unsigned long seconds = interval_us / 1000000;
        // Simple single-digit printing
        hal_uart_putc('0' + (char)seconds);
        hal_uart_puts(" second");
        if (seconds > 1) hal_uart_putc('s');
    } else if (interval_us >= 1000) {
        unsigned long ms = interval_us / 1000;
        hal_uart_putc('0' + (char)ms);
        hal_uart_puts(" ms");
    } else {
        hal_uart_puts("? us");
    }
    hal_uart_puts(")\n");
}

/**
 * Get current tick count
 */
unsigned long hal_timer_get_ticks(void) {
    return ticks;
}

/**
 * Set next timer interrupt
 */
void hal_timer_set_next(unsigned long interval_us) {
    unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;
    unsigned long current_time = read_time();
    unsigned long next_time = current_time + interval_ticks;
    
    // Use SSTC if available, otherwise use SBI
    if (has_sstc()) {
        sstc_set_timer(next_time);
    } else {
        sbi_set_timer(next_time);
    }
}

/**
 * Handle timer interrupt
 * 
 * This is called from the trap handler when a timer interrupt occurs.
 * It increments the tick counter, calls the scheduler for preemptive
 * multitasking, and schedules the next interrupt.
 */
void hal_timer_handle_interrupt(void) {
    // Increment tick counter
    ticks++;
    
    // Call scheduler for preemptive multitasking
    extern void schedule(void);
    schedule();
    
    // Schedule next interrupt using the configured interval
    hal_timer_set_next(timer_interval_us);
}
