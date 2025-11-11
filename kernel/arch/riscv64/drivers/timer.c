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
 * Initialize timer hardware and start periodic interrupts
 */
void hal_timer_init(unsigned long interval_us) {
    // Save the interval for later use
    timer_interval_us = interval_us;
    
    // Calculate ticks for the desired interval
    unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;
    
    // Set first timer interrupt using SBI
    unsigned long current_time = read_time();
    sbi_set_timer(current_time + interval_ticks);
    
    // Enable timer interrupts in sie (supervisor interrupt enable)
    unsigned long sie;
    asm volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1 << 5);  // STIE - Supervisor Timer Interrupt Enable
    asm volatile("csrw sie, %0" :: "r"(sie));
    
    // Enable interrupts globally in sstatus
    unsigned long sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus |= (1 << 1);  // SIE - Supervisor Interrupt Enable
    asm volatile("csrw sstatus, %0" :: "r"(sstatus));
    
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
    sbi_set_timer(current_time + interval_ticks);
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
