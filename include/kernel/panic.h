/*
 * Kernel Panic Handler
 * 
 * Provides fatal error handling and system halt functionality.
 */

#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

/**
 * Kernel panic - fatal error handler
 * 
 * This function is called when the kernel encounters an unrecoverable error.
 * It will:
 * 1. Disable interrupts
 * 2. Print the panic message and caller information
 * 3. Halt the system in an infinite loop
 * 
 * This function never returns.
 * 
 * @param fmt Format string (currently only supports plain strings)
 * 
 * Example usage:
 *   if (!critical_resource)
 *       kernel_panic("Failed to allocate critical resource");
 */
void kernel_panic(const char *message) __attribute__((noreturn));

/**
 * Assertion macro for kernel code
 * 
 * If the condition is false, triggers a kernel panic with the failed
 * condition as the message.
 * 
 * Example:
 *   KASSERT(ptr != NULL, "Pointer must not be NULL");
 */
#define KASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            kernel_panic("Assertion failed: " message); \
        } \
    } while (0)

/**
 * Bug detection macro
 * 
 * Marks code paths that should never be reached. If executed,
 * triggers a kernel panic.
 * 
 * Example:
 *   default:
 *       BUG("Unexpected state in switch");
 */
#define BUG(message) kernel_panic("BUG: " message)

/**
 * Not-yet-implemented marker
 * 
 * For features that are planned but not yet implemented.
 * Triggers a panic if the code path is executed.
 */
#define NOT_IMPLEMENTED() kernel_panic("NOT IMPLEMENTED")

#endif // KERNEL_PANIC_H
