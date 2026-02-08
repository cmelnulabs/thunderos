/**
 * RISC-V Supervisor Binary Interface (SBI)
 * 
 * Provides interface to machine-mode firmware.
 */

#include "arch/sbi.h"
#include "hal/hal_uart.h"
#include "mm/paging.h"

/**
 * Perform SBI ecall (inline assembly)
 */
sbi_ret_t sbi_ecall(long ext, long fid, long arg0, long arg1,
                    long arg2, long arg3, long arg4, long arg5)
{
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    register long a5 asm("a5") = arg5;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;
    
    asm volatile (
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );
    
    sbi_ret_t ret;
    ret.error = a0;
    ret.value = a1;
    return ret;
}

/**
 * Shutdown the system using SBI
 * 
 * Attempts multiple shutdown methods in order:
 * 1. QEMU test device (most reliable)
 * 2. SBI SRST extension
 * 3. Legacy SBI shutdown
 * 
 * If all methods fail, enters infinite loop with WFI.
 * This function does not return on success.
 */
void sbi_shutdown(void)
{
    hal_uart_puts("\n[SBI] Initiating system shutdown...\n");
    
    /* Try QEMU test device first (most reliable method) */
    /* Switch to kernel page table to access MMIO device */
    switch_to_kernel_page_table();
    
    volatile uint32_t *test_dev = (volatile uint32_t *)QEMU_TEST_DEVICE_ADDR;
    *test_dev = QEMU_TEST_DEVICE_EXIT_SUCCESS;
    
    /* If QEMU test device didn't work, try SBI SRST extension */
    hal_uart_puts("[SBI] Trying SBI shutdown...\n");
    sbi_ret_t ret = sbi_ecall(SBI_EXT_SRST, 0,
                              SBI_SRST_RESET_TYPE_SHUTDOWN,
                              SBI_SRST_RESET_REASON_NONE,
                              0, 0, 0, 0);
    
    /* If SRST not supported, try legacy shutdown */
    if (ret.error == SBI_ERR_NOT_SUPPORTED) {
        hal_uart_puts("[SBI] SRST not supported, trying legacy shutdown\n");
        sbi_ecall(SBI_EXT_LEGACY_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
    }
    
    /* If we get here, all shutdown methods failed */
    hal_uart_puts("[SBI] Shutdown failed, halting CPU\n");
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * Reboot the system using SBI
 * 
 * Attempts multiple reboot methods in order:
 * 1. QEMU test device
 * 2. SBI SRST extension (cold reboot)
 * 
 * If all methods fail, enters infinite loop with WFI.
 * This function does not return on success.
 */
void sbi_reboot(void)
{
    hal_uart_puts("\n[SBI] Initiating system reboot...\n");
    
    /* Try QEMU test device first */
    /* Switch to kernel page table to access MMIO device */
    switch_to_kernel_page_table();
    
    volatile uint32_t *test_dev = (volatile uint32_t *)QEMU_TEST_DEVICE_ADDR;
    *test_dev = QEMU_TEST_DEVICE_RESET;
    
    /* If QEMU test device didn't work, try SBI SRST extension for cold reboot */
    hal_uart_puts("[SBI] Trying SBI reboot...\n");
    sbi_ecall(SBI_EXT_SRST, 0,
              SBI_SRST_RESET_TYPE_COLD_REBOOT,
              SBI_SRST_RESET_REASON_NONE,
              0, 0, 0, 0);
    
    /* If we get here, reboot failed */
    hal_uart_puts("[SBI] Reboot failed, halting CPU\n");
    while (1) {
        asm volatile("wfi");
    }
}
