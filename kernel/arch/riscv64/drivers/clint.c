/*
 * RISC-V CLINT (Core-Local Interruptor) Driver Implementation
 * ThunderOS - RISC-V Operating System
 */

#include "arch/clint.h"

/* Helper macros for CLINT register access */
#define CLINT_REG_32(offset) ((volatile uint32_t *)(CLINT_BASE + (offset)))
#define CLINT_REG_64(offset) ((volatile uint64_t *)(CLINT_BASE + (offset)))

#define CLINT_MSIP_REG(hart) CLINT_REG_32(CLINT_MSIP_OFFSET + ((unsigned long)(hart) * 4UL))
#define CLINT_MTIMECMP_REG(hart) CLINT_REG_64(CLINT_MTIMECMP_OFFSET + ((unsigned long)(hart) * 8UL))
#define CLINT_MTIME_REG CLINT_REG_64(CLINT_MTIME_OFFSET)

/* CSR access helpers */
#define CSR_SIE 0x104
#define CSR_SIP 0x144

#define SIE_STIE (1UL << 5)  /* Supervisor Timer Interrupt Enable */
#define SIP_STIP (1UL << 5)  /* Supervisor Timer Interrupt Pending */

/*
 * Read a CSR register
 */
static inline uint64_t read_csr(uint32_t csr_number)
{
    uint64_t value = 0;
    
    if (csr_number == CSR_SIE)
    {
        __asm__ volatile("csrr %0, sie" : "=r"(value));
    }
    else if (csr_number == CSR_SIP)
    {
        __asm__ volatile("csrr %0, sip" : "=r"(value));
    }
    
    return value;
}

/*
 * Write a CSR register
 */
static inline void write_csr(uint32_t csr_number, uint64_t value)
{
    if (csr_number == CSR_SIE)
    {
        __asm__ volatile("csrw sie, %0" :: "r"(value));
    }
    else if (csr_number == CSR_SIP)
    {
        __asm__ volatile("csrw sip, %0" :: "r"(value));
    }
}

/*
 * Set bits in a CSR register
 */
static inline void set_csr_bits(uint32_t csr_number, uint64_t bits)
{
    if (csr_number == CSR_SIE)
    {
        __asm__ volatile("csrs sie, %0" :: "r"(bits));
    }
    else if (csr_number == CSR_SIP)
    {
        __asm__ volatile("csrs sip, %0" :: "r"(bits));
    }
}

/*
 * Clear bits in a CSR register
 */
static inline void clear_csr_bits(uint32_t csr_number, uint64_t bits)
{
    if (csr_number == CSR_SIE)
    {
        __asm__ volatile("csrc sie, %0" :: "r"(bits));
    }
    else if (csr_number == CSR_SIP)
    {
        __asm__ volatile("csrc sip, %0" :: "r"(bits));
    }
}

/*
 * Initialize the CLINT
 */
void clint_init(void)
{
    uint32_t hart_id = 0;  /* For now, we only support hart 0 */
    
    /* Clear any pending software interrupts */
    clint_clear_software_interrupt(hart_id);
    
    /* Disable timer interrupts initially */
    clint_disable_timer_interrupt();
    
    /* Set timer compare to maximum value to prevent immediate interrupts */
    *CLINT_MTIMECMP_REG(hart_id) = UINT64_MAX;
}

/*
 * Get the current time value from CLINT
 */
uint64_t clint_get_time(void)
{
    return *CLINT_MTIME_REG;
}

/*
 * Set the timer compare value to trigger an interrupt at a specific time
 */
void clint_set_timer(uint64_t time_value)
{
    uint32_t hart_id = 0;  /* For now, we only support hart 0 */
    
    *CLINT_MTIMECMP_REG(hart_id) = time_value;
}

/*
 * Add a delta to the current timer compare value
 */
void clint_add_timer(uint64_t time_delta)
{
    uint32_t hart_id = 0;  /* For now, we only support hart 0 */
    uint64_t current_compare = *CLINT_MTIMECMP_REG(hart_id);
    
    *CLINT_MTIMECMP_REG(hart_id) = current_compare + time_delta;
}

/*
 * Enable timer interrupts in supervisor mode
 */
void clint_enable_timer_interrupt(void)
{
    set_csr_bits(CSR_SIE, SIE_STIE);
}

/*
 * Disable timer interrupts in supervisor mode
 */
void clint_disable_timer_interrupt(void)
{
    clear_csr_bits(CSR_SIE, SIE_STIE);
}

/*
 * Trigger a software interrupt for a specific hart
 */
void clint_trigger_software_interrupt(uint32_t hart_id)
{
    *CLINT_MSIP_REG(hart_id) = 1;
}

/*
 * Clear a software interrupt for a specific hart
 */
void clint_clear_software_interrupt(uint32_t hart_id)
{
    *CLINT_MSIP_REG(hart_id) = 0;
}
