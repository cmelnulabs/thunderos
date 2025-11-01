/*
 * Trap handler for RISC-V
 */

#include "trap.h"
#include "hal/hal_uart.h"
#include "hal/hal_timer.h"
#include "kernel/syscall.h"

/* Forward declaration for external interrupt handler */
void handle_external_interrupt(void);

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
    
    hal_uart_puts(buf);
}

// Handle exceptions (synchronous traps)
static void handle_exception(struct trap_frame *tf, unsigned long cause) {
    // Check if this is an ECALL from user mode (syscall)
    if (cause == CAUSE_USER_ECALL) {
        // System call - pass syscall number and arguments from trap frame directly
        
        // Call syscall handler
        uint64_t ret = syscall_handler(tf->a7, tf->a0, tf->a1, tf->a2, tf->a3, tf->a4, tf->a5);
        
        // Store return value in a0
        tf->a0 = ret;
        
        // Advance sepc past the ECALL instruction (4 bytes)
        tf->sepc += 4;
        
        return;
    }
    
    // Not a syscall - handle as error
    hal_uart_puts("\n!!! EXCEPTION !!!\n");
    hal_uart_puts("Cause: ");
    
    if (cause < sizeof(exception_names) / sizeof(exception_names[0])) {
        hal_uart_puts(exception_names[cause]);
    } else {
        hal_uart_puts("Unknown");
    }
    
    hal_uart_puts("\nsepc:   ");
    print_hex(tf->sepc);
    hal_uart_puts("\nstval:  ");
    print_hex(read_stval());
    hal_uart_puts("\nscause: ");
    print_hex(cause);
    hal_uart_puts("\n");
    
    // Halt system
    hal_uart_puts("System halted.\n");
    while (1) {
        asm volatile("wfi");
    }
}

// Handle interrupts (asynchronous traps)
static void handle_interrupt(struct trap_frame *tf __attribute__((unused)), unsigned long cause) {
    cause &= ~INTERRUPT_BIT; // Remove interrupt bit
    
    switch (cause) {
        case IRQ_S_TIMER:
            // Handle timer interrupt
            hal_timer_handle_interrupt();
            // Call scheduler for preemptive multitasking
            extern void schedule(void);
            schedule();
            break;
        case IRQ_S_SOFT:
            hal_uart_puts("Software interrupt\n");
            break;
        case IRQ_S_EXTERNAL:
            // Handle external interrupt via PLIC
            handle_external_interrupt();
            break;
        default:
            hal_uart_puts("Unknown interrupt: ");
            print_hex(cause);
            hal_uart_puts("\n");
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
    
    hal_uart_puts("Trap handler initialized\n");
}
