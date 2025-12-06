/*
 * RISC-V PLIC (Platform-Level Interrupt Controller) Driver Implementation
 * ThunderOS - RISC-V Operating System
 */

#include "arch/plic.h"
#include <stddef.h>

/* Helper macros for PLIC register access */
#define PLIC_REG(offset) ((volatile uint32_t *)(PLIC_BASE + (offset)))
#define PLIC_PRIORITY_REG(irq) PLIC_REG(PLIC_PRIORITY_OFFSET + ((unsigned long)(irq) * 4UL))
#define PLIC_ENABLE_REG(context, word) PLIC_REG(PLIC_ENABLE_OFFSET + ((unsigned long)(context) * 0x80UL) + ((unsigned long)(word) * 4UL))
#define PLIC_THRESHOLD_REG(context) PLIC_REG(PLIC_THRESHOLD_OFFSET + ((unsigned long)(context) * 0x1000UL))
#define PLIC_CLAIM_REG(context) PLIC_REG(PLIC_CLAIM_OFFSET + ((unsigned long)(context) * 0x1000UL))

/* Number of words needed for enable bits (128 interrupts / 32 bits per word) */
#define PLIC_ENABLE_WORDS 4

/*
 * Initialize the PLIC
 */
void plic_init(void)
{
    uint32_t irq_number = 0;
    uint32_t word_index = 0;
    uint32_t context = PLIC_CONTEXT_SUPERVISOR_HART0;

    /* Disable all interrupts for supervisor context */
    for (word_index = 0; word_index < PLIC_ENABLE_WORDS; word_index++)
    {
        *PLIC_ENABLE_REG(context, word_index) = 0;
    }

    /* Set all interrupt priorities to 0 (disabled) */
    for (irq_number = 1; irq_number < PLIC_MAX_IRQ; irq_number++)
    {
        *PLIC_PRIORITY_REG(irq_number) = PLIC_PRIORITY_MIN;
    }

    /* Set priority threshold to 0 (accept all priorities) */
    plic_set_threshold(PLIC_PRIORITY_MIN, context);
}

/*
 * Set priority for a specific interrupt
 */
void plic_set_priority(uint32_t irq_number, uint32_t priority)
{
    if (irq_number == 0 || irq_number >= PLIC_MAX_IRQ)
    {
        return;  /* IRQ 0 is reserved, ignore invalid IRQ numbers */
    }

    if (priority > PLIC_PRIORITY_MAX)
    {
        priority = PLIC_PRIORITY_MAX;
    }

    *PLIC_PRIORITY_REG(irq_number) = priority;
}

/*
 * Enable a specific interrupt for a given context
 */
void plic_enable_interrupt(uint32_t irq_number, uint32_t context)
{
    uint32_t word_index = 0;
    uint32_t bit_index = 0;
    volatile uint32_t *enable_reg = NULL;
    uint32_t current_value = 0;

    if (irq_number == 0 || irq_number >= 128)
    {
        return;  /* IRQ 0 is reserved, ignore invalid IRQ numbers */
    }

    word_index = irq_number / 32;
    bit_index = irq_number % 32;
    enable_reg = PLIC_ENABLE_REG(context, word_index);
    current_value = *enable_reg;

    *enable_reg = current_value | (1U << bit_index);
}

/*
 * Disable a specific interrupt for a given context
 */
void plic_disable_interrupt(uint32_t irq_number, uint32_t context)
{
    uint32_t word_index = 0;
    uint32_t bit_index = 0;
    volatile uint32_t *enable_reg = NULL;
    uint32_t current_value = 0;

    if (irq_number == 0 || irq_number >= 128)
    {
        return;  /* IRQ 0 is reserved, ignore invalid IRQ numbers */
    }

    word_index = irq_number / 32;
    bit_index = irq_number % 32;
    enable_reg = PLIC_ENABLE_REG(context, word_index);
    current_value = *enable_reg;

    *enable_reg = current_value & ~(1U << bit_index);
}

/*
 * Set the priority threshold for a context
 * Interrupts with priority <= threshold are masked
 */
void plic_set_threshold(uint32_t threshold, uint32_t context)
{
    if (threshold > PLIC_PRIORITY_MAX)
    {
        threshold = PLIC_PRIORITY_MAX;
    }

    *PLIC_THRESHOLD_REG(context) = threshold;
}

/*
 * Claim the next pending interrupt
 * Returns the IRQ number, or 0 if no interrupt is pending
 */
uint32_t plic_claim_interrupt(uint32_t context)
{
    return *PLIC_CLAIM_REG(context);
}

/*
 * Signal completion of interrupt handling
 */
void plic_complete_interrupt(uint32_t irq_number, uint32_t context)
{
    *PLIC_CLAIM_REG(context) = irq_number;
}
