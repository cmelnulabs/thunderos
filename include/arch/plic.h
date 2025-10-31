/*
 * RISC-V PLIC (Platform-Level Interrupt Controller) Driver
 * ThunderOS - RISC-V Operating System
 */

#ifndef ARCH_PLIC_H
#define ARCH_PLIC_H

#include <stdint.h>

/* PLIC Memory-Mapped Registers Base Address (QEMU virt machine) */
#define PLIC_BASE 0x0C000000UL

/* PLIC Register Offsets */
#define PLIC_PRIORITY_OFFSET   0x000000UL
#define PLIC_PENDING_OFFSET    0x001000UL
#define PLIC_ENABLE_OFFSET     0x002000UL
#define PLIC_THRESHOLD_OFFSET  0x200000UL
#define PLIC_CLAIM_OFFSET      0x200004UL

/* Context for supervisor mode, hart 0 */
#define PLIC_CONTEXT_SUPERVISOR_HART0 1

/* Priority levels */
#define PLIC_PRIORITY_MIN 0
#define PLIC_PRIORITY_MAX 7

/* Public API */
void plic_init(void);
void plic_set_priority(uint32_t irq_number, uint32_t priority);
void plic_enable_interrupt(uint32_t irq_number, uint32_t context);
void plic_disable_interrupt(uint32_t irq_number, uint32_t context);
void plic_set_threshold(uint32_t threshold, uint32_t context);
uint32_t plic_claim_interrupt(uint32_t context);
void plic_complete_interrupt(uint32_t irq_number, uint32_t context);

#endif // ARCH_PLIC_H
