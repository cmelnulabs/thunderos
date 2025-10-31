/*
 * RISC-V CLINT (Core-Local Interruptor) Driver
 * ThunderOS - RISC-V Operating System
 */

#ifndef ARCH_CLINT_H
#define ARCH_CLINT_H

#include <stdint.h>

/* CLINT Memory-Mapped Registers Base Address (QEMU virt machine) */
#define CLINT_BASE 0x02000000UL

/* CLINT Register Offsets */
#define CLINT_MSIP_OFFSET    0x0000UL  /* Machine Software Interrupt Pending */
#define CLINT_MTIMECMP_OFFSET 0x4000UL  /* Machine Time Compare */
#define CLINT_MTIME_OFFSET   0xBFF8UL  /* Machine Time Register */

/* Timer frequency (10 MHz on QEMU) */
#define CLINT_TIMER_FREQ 10000000UL

/* Public API */
void clint_init(void);
uint64_t clint_get_time(void);
void clint_set_timer(uint64_t time_value);
void clint_add_timer(uint64_t time_delta);
void clint_enable_timer_interrupt(void);
void clint_disable_timer_interrupt(void);
void clint_trigger_software_interrupt(uint32_t hart_id);
void clint_clear_software_interrupt(uint32_t hart_id);

#endif // ARCH_CLINT_H
