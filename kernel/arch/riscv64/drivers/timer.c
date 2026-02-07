/*
 * RISC-V Timer Driver
 * 
 * Uses CLINT MMIO for timer interrupts (compatible with all RISC-V implementations).
 * Goal: Get continuous timer interrupts working without Sstc extension.
 */

#include "hal/hal_timer.h"
#include "hal/hal_uart.h"
#include "drivers/vterm.h"
#include "arch/clint.h"

static volatile unsigned long ticks = 0;
static unsigned long timer_interval_us = 0;

void hal_timer_init(unsigned long interval_us) {
    timer_interval_us = interval_us;
    unsigned long interval_ticks = (CLINT_TIMER_FREQ * interval_us) / 1000000;
    
    // Initialize CLINT and set first timer deadline
    clint_init();
    clint_add_timer(interval_ticks);
    clint_enable_timer_interrupt();
}

unsigned long hal_timer_get_ticks(void) {
    return ticks;
}

void hal_timer_set_next(unsigned long interval_us) {
    unsigned long interval_ticks = (CLINT_TIMER_FREQ * interval_us) / 1000000;
    clint_add_timer(interval_ticks);
}

void hal_timer_handle_interrupt(void) {
    ticks++;
    
    // Schedule next interrupt BEFORE calling scheduler
    hal_timer_set_next(timer_interval_us);
    
    // Poll for keyboard input (VT switching, etc.)
    // This allows switching terminals even when processes don't read input
    if (vterm_available()) {
        vterm_poll_input();
    }
    
    // Preemptive multitasking
    extern void schedule(void);
    schedule();
}
