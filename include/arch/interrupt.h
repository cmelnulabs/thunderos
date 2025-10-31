/*
 * RISC-V Interrupt Management API
 * ThunderOS - RISC-V Operating System
 */

#ifndef ARCH_INTERRUPT_H
#define ARCH_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of interrupt sources */
#define MAX_INTERRUPT_SOURCES 128

/* Interrupt priority levels (1-7, 0 = disabled) */
#define IRQ_PRIORITY_DISABLED 0
#define IRQ_PRIORITY_LOWEST   1
#define IRQ_PRIORITY_LOW      2
#define IRQ_PRIORITY_NORMAL   3
#define IRQ_PRIORITY_HIGH     5
#define IRQ_PRIORITY_HIGHEST  7

/* Interrupt handler function type */
typedef void (*interrupt_handler_t)(void);

/* Public API */
void interrupt_init(void);
void interrupt_enable(void);
void interrupt_disable(void);
bool interrupt_register_handler(uint32_t irq_number, interrupt_handler_t handler);
void interrupt_unregister_handler(uint32_t irq_number);
void interrupt_set_priority(uint32_t irq_number, uint32_t priority);
void interrupt_enable_irq(uint32_t irq_number);
void interrupt_disable_irq(uint32_t irq_number);

#endif // ARCH_INTERRUPT_H
