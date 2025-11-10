/*
 * Memory Barriers for RISC-V
 * 
 * Provides memory ordering primitives for device I/O and synchronization.
 * RISC-V uses fence instructions to enforce memory ordering constraints.
 */

#ifndef BARRIER_H
#define BARRIER_H

/**
 * Full memory barrier
 * 
 * Ensures all memory operations before this point complete before
 * any memory operations after this point begin.
 * 
 * Use this when:
 * - Writing to device registers (ensure writes complete)
 * - Reading from device registers (ensure reads happen in order)
 * - Synchronizing between CPU and device DMA
 */
static inline void memory_barrier(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

/**
 * Write memory barrier
 * 
 * Ensures all write operations before this point complete before
 * any write operations after this point begin.
 * 
 * Use this when:
 * - Writing multiple device registers in sequence
 * - Setting up DMA descriptors before notifying device
 */
static inline void write_barrier(void) {
    asm volatile("fence w, w" ::: "memory");
}

/**
 * Read memory barrier
 * 
 * Ensures all read operations before this point complete before
 * any read operations after this point begin.
 * 
 * Use this when:
 * - Reading multiple device registers in sequence
 * - Reading DMA buffers after device signals completion
 */
static inline void read_barrier(void) {
    asm volatile("fence r, r" ::: "memory");
}

/**
 * I/O memory barrier
 * 
 * Ensures all memory-mapped I/O operations before this point complete
 * before any I/O operations after this point begin.
 * 
 * This is equivalent to a full memory barrier on RISC-V.
 */
static inline void io_barrier(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

/**
 * Data Memory Barrier (DMB)
 * 
 * Ensures all memory accesses before this point are observed by
 * other agents (CPUs, devices) before any accesses after this point.
 * 
 * Critical for DMA operations where device reads/writes memory.
 */
static inline void data_memory_barrier(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

/**
 * Data Synchronization Barrier (DSB)
 * 
 * Ensures all memory accesses and cache operations before this point
 * complete before continuing execution.
 * 
 * Use before signaling a device or after receiving device notification.
 */
static inline void data_sync_barrier(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

/**
 * Instruction barrier
 * 
 * Ensures instruction fetch ordering. Use after modifying code in memory
 * (e.g., JIT compilation, dynamic loading).
 */
static inline void instruction_barrier(void) {
    asm volatile("fence.i" ::: "memory");
}

/**
 * Compiler barrier
 * 
 * Prevents compiler from reordering memory accesses across this point.
 * Does NOT emit any instructions, only affects compiler optimization.
 * 
 * Use when you need ordering guarantees but don't need hardware barriers.
 */
static inline void compiler_barrier(void) {
    asm volatile("" ::: "memory");
}

/**
 * Read volatile memory location with barrier
 * 
 * Helper for reading device registers with proper ordering.
 */
static inline uint32_t read32_barrier(volatile uint32_t *addr) {
    uint32_t value = *addr;
    read_barrier();
    return value;
}

/**
 * Write volatile memory location with barrier
 * 
 * Helper for writing device registers with proper ordering.
 */
static inline void write32_barrier(volatile uint32_t *addr, uint32_t value) {
    write_barrier();
    *addr = value;
}

/**
 * Read-modify-write with barriers
 * 
 * Atomic read-modify-write for device registers.
 */
static inline void rmw32_barrier(volatile uint32_t *addr, uint32_t mask, uint32_t value) {
    memory_barrier();
    uint32_t old = *addr;
    *addr = (old & ~mask) | (value & mask);
    memory_barrier();
}

#endif // BARRIER_H
