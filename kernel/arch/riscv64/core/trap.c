/*
 * Trap handler for RISC-V
 */

#include "trap.h"
#include "hal/hal_uart.h"
#include "hal/hal_timer.h"
#include "kernel/syscall.h"
#include "kernel/process.h"
#include "kernel/kstring.h"

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

static inline unsigned long read_sstatus(void) {
    unsigned long x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

// Check if trap occurred from user mode
static int trap_from_user_mode(void) {
    // SPP bit (bit 8) indicates previous privilege level
    // SPP=0 means we were in user mode, SPP=1 means supervisor mode
    return (read_sstatus() & (1 << 8)) == 0;
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
    
    // Check if exception occurred in user mode
    if (trap_from_user_mode()) {
        // Exception in user process - terminate the process
        hal_uart_puts("\n!!! USER PROCESS EXCEPTION !!!\n");
        hal_uart_puts("Process: ");
        
        // Get current process info
        extern struct process *process_current(void);
        struct process *proc = process_current();
        if (proc) {
            kprint_dec(proc->pid);
            hal_uart_puts(" (");
            hal_uart_puts(proc->name);
            hal_uart_puts(")");
        } else {
            hal_uart_puts("unknown");
        }
        hal_uart_puts("\n");
        
        hal_uart_puts("Exception: ");
        if (cause < sizeof(exception_names) / sizeof(exception_names[0])) {
            hal_uart_puts(exception_names[cause]);
        } else {
            hal_uart_puts("Unknown");
        }
        hal_uart_puts("\n");
        
        hal_uart_puts("sepc:   ");
        print_hex(tf->sepc);
        hal_uart_puts("\nstval:  ");
        print_hex(read_stval());
        hal_uart_puts("\n");
        
        // Terminate the user process
        extern void process_exit(int) __attribute__((noreturn));
        process_exit(-1);  // Exit with error code
        
        return;  // Should not reach here
    }
    
    // Exception in kernel mode - print info and halt
    hal_uart_puts("\n!!! KERNEL EXCEPTION !!!\n");
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
