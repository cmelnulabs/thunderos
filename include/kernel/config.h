/*
 * Kernel Configuration
 * 
 * System-wide configuration constants and parameters.
 */

#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

// Timer configuration
// Timer interrupt interval in microseconds
// 100ms = 100,000 microseconds
#define TIMER_INTERVAL_US 100000

// Hardware and memory layout constants (QEMU virt machine)
#define KERNEL_LOAD_ADDRESS 0x80200000  // From linker script
#define RAM_START_ADDRESS 0x80000000    // Physical RAM base
#define RAM_END_ADDRESS 0x88000000      // 128MB RAM end
#define RAM_SIZE_MB 128                 // Total RAM size

#endif // KERNEL_CONFIG_H
