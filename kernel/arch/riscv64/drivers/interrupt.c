/*
 * RISC-V Interrupt Management Implementation
 * ThunderOS - RISC-V Operating System
 */

#include "arch/interrupt.h"
#include "arch/plic.h"
#include "arch/clint.h"
#include "trap.h"
#include <stddef.h>

/* Interrupt handler table */
static interrupt_handler_t interrupt_handlers[MAX_INTERRUPT_SOURCES] = {NULL};

/* CSR definitions for interrupt enable/disable */
#define CSR_SSTATUS 0x100
#define SSTATUS_SIE (1UL << 1)  /* Supervisor Interrupt Enable */

/* Forward declarations */
static void enable_supervisor_interrupts(void);
static void disable_supervisor_interrupts(void);
static void configure_default_priorities(void);

/*
 * Enable supervisor interrupts globally
 */
static void enable_supervisor_interrupts(void)
{
    __asm__ volatile("csrs sstatus, %0" :: "r"(SSTATUS_SIE));
}

/*
 * Disable supervisor interrupts globally
 */
static void disable_supervisor_interrupts(void)
{
    __asm__ volatile("csrc sstatus, %0" :: "r"(SSTATUS_SIE));
}

/*
 * Configure default interrupt priorities
 */
static void configure_default_priorities(void)
{
    uint32_t irq_number = 0;
    
    /* Set all interrupts to normal priority by default */
    for (irq_number = 1; irq_number < MAX_INTERRUPT_SOURCES; irq_number++)
    {
        plic_set_priority(irq_number, IRQ_PRIORITY_NORMAL);
    }
}

/*
 * Initialize the interrupt subsystem
 */
void interrupt_init(void)
{
    uint32_t handler_index = 0;
    
    /* Clear all registered handlers */
    for (handler_index = 0; handler_index < MAX_INTERRUPT_SOURCES; handler_index++)
    {
        interrupt_handlers[handler_index] = NULL;
    }
    
    /* Initialize PLIC (Platform-Level Interrupt Controller) */
    plic_init();
    
    /* Initialize CLINT (Core-Local Interruptor) */
    clint_init();
    
    /* Configure default priorities for all interrupts */
    configure_default_priorities();
    
    /* Initialize trap handling */
    /* trap_init(); -- redundant, already called in kernel_main() */
}

/*
 * Enable interrupts globally
 */
void interrupt_enable(void)
{
    enable_supervisor_interrupts();
}

/*
 * Disable interrupts globally
 */
void interrupt_disable(void)
{
    disable_supervisor_interrupts();
}

/*
 * Register an interrupt handler for a specific IRQ
 */
bool interrupt_register_handler(uint32_t irq_number, interrupt_handler_t handler)
{
    if (irq_number == 0 || irq_number >= MAX_INTERRUPT_SOURCES)
    {
        return false;  /* Invalid IRQ number */
    }
    
    if (handler == NULL)
    {
        return false;  /* Invalid handler */
    }
    
    if (interrupt_handlers[irq_number] != NULL)
    {
        return false;  /* Handler already registered */
    }
    
    interrupt_handlers[irq_number] = handler;
    return true;
}

/*
 * Unregister an interrupt handler
 */
void interrupt_unregister_handler(uint32_t irq_number)
{
    if (irq_number == 0 || irq_number >= MAX_INTERRUPT_SOURCES)
    {
        return;  /* Invalid IRQ number */
    }
    
    interrupt_handlers[irq_number] = NULL;
}

/*
 * Set the priority for a specific interrupt
 */
void interrupt_set_priority(uint32_t irq_number, uint32_t priority)
{
    if (irq_number == 0 || irq_number >= MAX_INTERRUPT_SOURCES)
    {
        return;  /* Invalid IRQ number */
    }
    
    if (priority > IRQ_PRIORITY_HIGHEST)
    {
        priority = IRQ_PRIORITY_HIGHEST;
    }
    
    plic_set_priority(irq_number, priority);
}

/*
 * Enable a specific IRQ
 */
void interrupt_enable_irq(uint32_t irq_number)
{
    uint32_t context = PLIC_CONTEXT_SUPERVISOR_HART0;
    
    if (irq_number == 0 || irq_number >= MAX_INTERRUPT_SOURCES)
    {
        return;  /* Invalid IRQ number */
    }
    
    plic_enable_interrupt(irq_number, context);
}

/*
 * Disable a specific IRQ
 */
void interrupt_disable_irq(uint32_t irq_number)
{
    uint32_t context = PLIC_CONTEXT_SUPERVISOR_HART0;
    
    if (irq_number == 0 || irq_number >= MAX_INTERRUPT_SOURCES)
    {
        return;  /* Invalid IRQ number */
    }
    
    plic_disable_interrupt(irq_number, context);
}

/*
 * Handle external interrupts from PLIC
 * Called by the trap handler when an external interrupt occurs
 */
void handle_external_interrupt(void)
{
    uint32_t context = PLIC_CONTEXT_SUPERVISOR_HART0;
    uint32_t irq_number = 0;
    interrupt_handler_t handler = NULL;
    
    /* Claim the interrupt */
    irq_number = plic_claim_interrupt(context);
    
    if (irq_number == 0)
    {
        return;  /* No interrupt pending */
    }
    
    /* Get the registered handler */
    handler = interrupt_handlers[irq_number];
    
    /* Call the handler if registered */
    if (handler != NULL)
    {
        handler();
    }
    
    /* Complete the interrupt */
    plic_complete_interrupt(irq_number, context);
}
