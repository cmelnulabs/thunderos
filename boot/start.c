/*
 * M-mode initialization for ThunderOS
 * Based on xv6-riscv approach
 * 
 * This code runs in machine mode before transitioning to supervisor mode.
 * It configures all the M-mode CSRs that OpenSBI would normally set up.
 */

#include <limits.h>

// CSR read/write macros
#define r_mstatus() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mstatus" : "=r"(__tmp)); \
  __tmp; })

#define w_mstatus(x) ({ \
  asm volatile("csrw mstatus, %0" :: "r"(x)); })

#define r_mepc() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mepc" : "=r"(__tmp)); \
  __tmp; })

#define w_mepc(x) ({ \
  asm volatile("csrw mepc, %0" :: "r"(x)); })

#define r_mie() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mie" : "=r"(__tmp)); \
  __tmp; })

#define w_mie(x) ({ \
  asm volatile("csrw mie, %0" :: "r"(x)); })

#define r_sie() ({ unsigned long __tmp; \
  asm volatile("csrr %0, sie" : "=r"(__tmp)); \
  __tmp; })

#define w_sie(x) ({ \
  asm volatile("csrw sie, %0" :: "r"(x)); })

#define r_medeleg() ({ unsigned long __tmp; \
  asm volatile("csrr %0, medeleg" : "=r"(__tmp)); \
  __tmp; })

#define w_medeleg(x) ({ \
  asm volatile("csrw medeleg, %0" :: "r"(x)); })

#define r_mideleg() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mideleg" : "=r"(__tmp)); \
  __tmp; })

#define w_mideleg(x) ({ \
  asm volatile("csrw mideleg, %0" :: "r"(x)); })

#define w_satp(x) ({ \
  asm volatile("csrw satp, %0" :: "r"(x)); })

#define w_pmpaddr0(x) ({ \
  asm volatile("csrw pmpaddr0, %0" :: "r"(x)); })

#define w_pmpcfg0(x) ({ \
  asm volatile("csrw pmpcfg0, %0" :: "r"(x)); })

#define r_mhartid() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mhartid" : "=r"(__tmp)); \
  __tmp; })

#define w_tp(x) ({ \
  asm volatile("mv tp, %0" :: "r"(x)); })

#define r_menvcfg() ({ unsigned long __tmp; \
  asm volatile("csrr %0, menvcfg" : "=r"(__tmp)); \
  __tmp; })

#define w_menvcfg(x) ({ \
  asm volatile("csrw menvcfg, %0" :: "r"(x)); })

#define r_mcounteren() ({ unsigned long __tmp; \
  asm volatile("csrr %0, mcounteren" : "=r"(__tmp)); \
  __tmp; })

#define w_mcounteren(x) ({ \
  asm volatile("csrw mcounteren, %0" :: "r"(x)); })

// SSTC extension - stimecmp CSR (0x14D)
#define w_stimecmp(x) ({ \
  asm volatile("csrw 0x14D, %0" :: "r"(x)); })

// mstatus bits
#define MSTATUS_MPP_MASK (0b11L << 11)      // Bits 12:11 mask
#define MSTATUS_MPP_M    (0b11L << 11)      // MPP = 11 (Machine mode)
#define MSTATUS_MPP_S    (0b01L << 11)      // MPP = 01 (Supervisor mode)
#define MSTATUS_MPP_U    (0b00L << 11)      // MPP = 00 (User mode)
#define MSTATUS_MIE      (1L << 3)          // Bit 3: Machine Interrupt Enable

// MIE register bits
#define MIE_MEIE (0b1L << 11)  // Bit 11: M-mode external interrupt enable
#define MIE_MTIE (0b1L << 7)   // Bit 7:  M-mode timer interrupt enable
#define MIE_MSIE (0b1L << 3)   // Bit 3:  M-mode software interrupt enable
#define MIE_SEIE (0b1L << 9)   // Bit 9:  S-mode external interrupt enable
#define MIE_STIE (0b1L << 5)   // Bit 5:  S-mode timer interrupt enable
#define MIE_SSIE (0b1L << 1)   // Bit 1:  S-mode software interrupt enable

// SIE register bits
#define SIE_SEIE (0b1L << 9)   // Bit 9: S-mode external interrupt enable
#define SIE_STIE (0b1L << 5)   // Bit 5: S-mode timer interrupt enable
#define SIE_SSIE (0b1L << 1)   // Bit 1: S-mode software interrupt enable

// Forward declaration
void kernel_main(void);
void timerinit(void);

// Simple UART puts for M-mode debugging
static void m_uart_putc(char c) {
    volatile unsigned int *uart = (volatile unsigned int *)0x10000000;
    *uart = c;
}

static void m_uart_puts(const char *s) {
    while (*s) {
        m_uart_putc(*s++);
    }
}

/*
 * Machine-mode timer initialization
 * This is called from start() in M-mode
 */
void timerinit(void) {
    // Enable supervisor timer interrupt in M-mode delegation
    unsigned long mie = r_mie();
    mie |= MIE_STIE;  // Enable supervisor timer interrupts
    w_mie(mie);
    
    // Enable Sstc extension (Supervisor Timer Compare)
    // This allows S-mode to write stimecmp CSR directly
    unsigned long menvcfg = r_menvcfg();
    menvcfg |= (1UL << 63);  // STCE bit - enable SSTC
    w_menvcfg(menvcfg);
    
    // Allow S-mode to access time CSR
    unsigned long mcounteren = r_mcounteren();
    mcounteren |= (1 << 1);  // TM bit - allow time CSR access
    w_mcounteren(mcounteren);
    
    // Set stimecmp to maximum value to prevent spurious interrupts
    // S-mode will program this properly when ready
    unsigned long max_time = ULONG_MAX;
    w_stimecmp(max_time);
    
    // Enable software and external interrupts in sie, but NOT timer yet
    // Timer will be enabled by hal_timer_init() when it's ready
    unsigned long sie = 0x222;  // Bits 1, 5, 9 (software, timer, external)
    sie &= ~(1 << 5);  // Clear STIE - timer interrupt will be enabled later
    w_sie(sie);
}

/*
 * M-mode entry point
 * Called from entry.S after setting up stack
 * Configures M-mode CSRs and transitions to S-mode
 */
void start(void)
{
    unsigned long x = 0;
    
    m_uart_puts("\n[M-MODE] ThunderOS starting in M-mode\n");
    
    // Set M Previous Privilege mode to Supervisor (for mret)
    // This determines what privilege mode we'll be in after mret
    x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;    // Clear MPP field
    x |= MSTATUS_MPP_S;         // Set MPP to Supervisor mode
    w_mstatus(x);
    
    m_uart_puts("[M-MODE] Set MPP=S-mode\n");
    
    // Set M Exception Program Counter to kernel_main
    // After mret, PC will be set to this address
    w_mepc((unsigned long)kernel_main);
    
    // Disable paging initially (will be enabled in kernel_main)
    w_satp(0);
    
    // Delegate all exceptions to supervisor mode
    // This allows S-mode to handle page faults, illegal instructions, etc.
    w_medeleg(0xffff);
    
    // Delegate all interrupts to supervisor mode
    // Bits: 15-0 correspond to interrupts 15-0
    // Bit 5 = S-mode timer interrupt
    // Bit 9 = S-mode external interrupt
    // Bit 1 = S-mode software interrupt
    w_mideleg(0xffff);
    
    // Enable supervisor-mode interrupts in SIE
    // This will allow interrupts once we're in S-mode
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    
    // Configure Physical Memory Protection (PMP)
    // Give supervisor mode access to all physical memory
    // pmpaddr0 = 0x3fffffffffffff (all 56 bits set)
    // pmpcfg0 = 0xf (R=1, W=1, X=1, A=01 TOR mode)
    w_pmpaddr0(0x3fffffffffffffUL);
    w_pmpcfg0(0xf);
    
    // Initialize timer in M-mode
    // This sets up mie.STIE, menvcfg.STCE, and mcounteren
    timerinit();
    
    // Store hartid in tp register
    // This allows S-mode code to identify which hart it's running on
    int id = r_mhartid();
    w_tp(id);
    
    // Switch to supervisor mode and jump to kernel_main()
    // This sets:
    //   - privilege mode = MPP (Supervisor)
    //   - PC = mepc (kernel_main)
    asm volatile("mret");
    
    // Never reach here
    while(1);
}
