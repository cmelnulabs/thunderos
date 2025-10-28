/*
 * CLINT driver implementation for RISC-V
 * Uses SBI (Supervisor Binary Interface) for timer on QEMU
 */

#include "clint.h"
#include "hal/hal_uart.h"

// Timer frequency on QEMU (10 MHz)
#define TIMER_FREQ 10000000UL

// How often to trigger timer interrupt (in microseconds)
#define TIMER_INTERVAL_US 1000000  // 1 second = 1,000,000 microseconds

// Global tick counter
static volatile unsigned long ticks = 0;

// SBI call numbers
#define SBI_SET_TIMER 0
#define SBI_EXT_TIME 0x54494D45  // "TIME"

// Make SBI call
static inline void sbi_set_timer(unsigned long stime_value) {
    register unsigned long a0 asm("a0") = stime_value;
    register unsigned long a7 asm("a7") = SBI_SET_TIMER;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

// Read time CSR
static inline unsigned long read_time(void) {
    unsigned long time;
    asm volatile("rdtime %0" : "=r"(time));
    return time;
}

// Initialize CLINT
void clint_init(void) {
    // Calculate ticks for the desired interval
    unsigned long interval = (TIMER_FREQ * TIMER_INTERVAL_US) / 1000000;
    
    // Set first timer interrupt using SBI
    unsigned long current_time = read_time();
    sbi_set_timer(current_time + interval);
    
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
    
    hal_uart_puts("CLINT timer initialized (interval: ");
    // Simple number printing
    if (TIMER_INTERVAL_US >= 1000000) {
        hal_uart_putc('1');
        hal_uart_puts(" second)\n");
    } else {
        hal_uart_puts("? ms)\n");
    }
}

// Get current tick count
unsigned long clint_get_ticks(void) {
    return ticks;
}

// Set next timer interrupt
void clint_set_timer(unsigned long ticks_from_now) {
    unsigned long interval = (TIMER_FREQ * ticks_from_now) / 1000000;
    unsigned long current_time = read_time();
    sbi_set_timer(current_time + interval);
}

// Handle timer interrupt
void clint_handle_timer(void) {
    // Increment tick counter
    ticks++;
    
    // Print tick (for debugging)
    hal_uart_puts("Tick: ");
    
    // Simple number printing (just print the tick count)
    unsigned long t = ticks;
    if (t == 0) {
        hal_uart_putc('0');
    } else {
        char buf[20];
        int i = 0;
        while (t > 0) {
            buf[i++] = '0' + (t % 10);
            t /= 10;
        }
        while (i > 0) {
            hal_uart_putc(buf[--i]);
        }
    }
    hal_uart_puts("\n");
    
    // Schedule next interrupt (1 second from now)
    clint_set_timer(TIMER_INTERVAL_US);
}
