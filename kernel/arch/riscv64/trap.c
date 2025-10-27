/*
 * Trap handler for RISC-V
 */

#include "trap.h"
#include "uart.h"

// CSR read helpers
static inline unsigned long read_scause(void) {
    unsigned long x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

static inline unsigned long read_stval(void) {
    unsigned long x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

static inline unsigned long read_sepc(void) {
    unsigned long x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

// Exception names for debugging
static const char *exception_names[] = {
    "Instruction address misaligned",
    "Instruction access fault",
    "Illegal instruction",
    "Breakpoint",
    "Load address misaligned",
    "Load access fault",
    "Store/AMO address misaligned",
    "Store/AMO access fault",
    "Environment call from U-mode",
    "Environment call from S-mode",
    "Reserved",
    "Environment call from M-mode",
    "Instruction page fault",
    "Load page fault",
    "Reserved",
    "Store/AMO page fault"
};

// Print a hex number
static void print_hex(unsigned long val) {
    char buf[19]; // "0x" + 16 hex digits + null
    buf[0] = '0';
    buf[1] = 'x';
    
    for (int i = 15; i >= 0; i--) {
        int digit = (val >> (i * 4)) & 0xF;
        buf[17 - i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
    }
    buf[18] = '\0';
    
    uart_puts(buf);
}

// Handle exceptions (synchronous traps)
static void handle_exception(struct trap_frame *tf, unsigned long cause) {
    uart_puts("\n!!! EXCEPTION !!!\n");
    uart_puts("Cause: ");
    
    if (cause < sizeof(exception_names) / sizeof(exception_names[0])) {
        uart_puts(exception_names[cause]);
    } else {
        uart_puts("Unknown");
    }
    
    uart_puts("\nsepc:   ");
    print_hex(tf->sepc);
    uart_puts("\nstval:  ");
    print_hex(read_stval());
    uart_puts("\nscause: ");
    print_hex(cause);
    uart_puts("\n");
    
    // Halt system
    uart_puts("System halted.\n");
    while (1) {
        asm volatile("wfi");
    }
}

// Handle interrupts (asynchronous traps)
static void handle_interrupt(struct trap_frame *tf, unsigned long cause) {
    cause &= ~INTERRUPT_BIT; // Remove interrupt bit
    
    uart_puts("Interrupt: ");
    print_hex(cause);
    uart_puts("\n");
    
    switch (cause) {
        case IRQ_S_TIMER:
            uart_puts("Timer interrupt\n");
            break;
        case IRQ_S_SOFT:
            uart_puts("Software interrupt\n");
            break;
        case IRQ_S_EXTERNAL:
            uart_puts("External interrupt\n");
            break;
        default:
            uart_puts("Unknown interrupt\n");
            break;
    }
}

// Main trap handler called from trap.S
void trap_handler(struct trap_frame *tf) {
    unsigned long cause = read_scause();
    
    if (cause & INTERRUPT_BIT) {
        // Asynchronous trap (interrupt)
        handle_interrupt(tf, cause);
    } else {
        // Synchronous trap (exception)
        handle_exception(tf, cause);
    }
}

// Initialize trap handling
void trap_init(void) {
    extern void trap_vector(void);
    
    // Set stvec to point to our trap handler
    // Mode: Direct (0) - all traps set pc to BASE
    asm volatile("csrw stvec, %0" :: "r"((unsigned long)trap_vector));
    
    uart_puts("Trap handler initialized\n");
}
