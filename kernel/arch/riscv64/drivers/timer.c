/*
 * RISC-V Timer Driver - Fresh Implementation
 * 
 * Simple timer interrupt implementation using SSTC extension.
 * Goal: Get continuous 100ms timer interrupts working.
 */

#include "hal/hal_timer.h"
#include "hal/hal_uart.h"

#define TIMER_FREQ 10000000UL  // 10 MHz

static volatile unsigned long ticks = 0;
static unsigned long timer_interval_us = 0;

static inline unsigned long read_time(void) {
    unsigned long time;
    asm volatile("rdtime %0" : "=r"(time));
    return time;
}

static inline void write_stimecmp(unsigned long value) {
    asm volatile("csrw 0x14D, %0" :: "r"(value));
}

void hal_timer_init(unsigned long interval_us) {
    timer_interval_us = interval_us;
    unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;
    unsigned long next_time = read_time() + interval_ticks;
    
    // Set first timer deadline
    write_stimecmp(next_time);
    
    // Enable timer interrupts in sie
    unsigned long sie;
    asm volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1 << 5);  // STIE bit
    asm volatile("csrw sie, %0" :: "r"(sie));
}

unsigned long hal_timer_get_ticks(void) {
    return ticks;
}

void hal_timer_set_next(unsigned long interval_us) {
    unsigned long interval_ticks = (TIMER_FREQ * interval_us) / 1000000;
    unsigned long next_time = read_time() + interval_ticks;
    write_stimecmp(next_time);
}

void hal_timer_handle_interrupt(void) {
    ticks++;
    
    // Schedule next interrupt BEFORE calling scheduler
    hal_timer_set_next(timer_interval_us);
    
    // Preemptive multitasking
    extern void schedule(void);
    schedule();
}
