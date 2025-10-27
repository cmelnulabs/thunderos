/*
 * Trap handling for RISC-V
 */

#ifndef TRAP_H
#define TRAP_H

// RISC-V exception causes (scause register)
#define CAUSE_MISALIGNED_FETCH    0
#define CAUSE_FETCH_ACCESS        1
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT          3
#define CAUSE_MISALIGNED_LOAD     4
#define CAUSE_LOAD_ACCESS         5
#define CAUSE_MISALIGNED_STORE    6
#define CAUSE_STORE_ACCESS        7
#define CAUSE_USER_ECALL          8
#define CAUSE_SUPERVISOR_ECALL    9
#define CAUSE_MACHINE_ECALL       11
#define CAUSE_FETCH_PAGE_FAULT    12
#define CAUSE_LOAD_PAGE_FAULT     13
#define CAUSE_STORE_PAGE_FAULT    15

// Interrupt bit in scause
#define INTERRUPT_BIT (1UL << 63)

// RISC-V interrupt causes
#define IRQ_S_SOFT    1
#define IRQ_S_TIMER   5
#define IRQ_S_EXTERNAL 9

// Trap frame structure - saved by trap handler
struct trap_frame {
    unsigned long ra;   // x1: return address
    unsigned long sp;   // x2: stack pointer
    unsigned long gp;   // x3: global pointer
    unsigned long tp;   // x4: thread pointer
    unsigned long t0;   // x5: temporary
    unsigned long t1;   // x6: temporary
    unsigned long t2;   // x7: temporary
    unsigned long s0;   // x8: saved register / frame pointer
    unsigned long s1;   // x9: saved register
    unsigned long a0;   // x10: argument/return value
    unsigned long a1;   // x11: argument/return value
    unsigned long a2;   // x12: argument
    unsigned long a3;   // x13: argument
    unsigned long a4;   // x14: argument
    unsigned long a5;   // x15: argument
    unsigned long a6;   // x16: argument
    unsigned long a7;   // x17: argument
    unsigned long s2;   // x18: saved register
    unsigned long s3;   // x19: saved register
    unsigned long s4;   // x20: saved register
    unsigned long s5;   // x21: saved register
    unsigned long s6;   // x22: saved register
    unsigned long s7;   // x23: saved register
    unsigned long s8;   // x24: saved register
    unsigned long s9;   // x25: saved register
    unsigned long s10;  // x26: saved register
    unsigned long s11;  // x27: saved register
    unsigned long t3;   // x28: temporary
    unsigned long t4;   // x29: temporary
    unsigned long t5;   // x30: temporary
    unsigned long t6;   // x31: temporary
    unsigned long sepc; // Supervisor exception program counter
    unsigned long sstatus; // Supervisor status register
};

// Function prototypes
void trap_init(void);
void trap_handler(struct trap_frame *tf);

#endif // TRAP_H
