/**
 * RISC-V Supervisor Binary Interface (SBI)
 * 
 * Provides interface to machine-mode firmware for system operations.
 */

#ifndef ARCH_SBI_H
#define ARCH_SBI_H

#include <stdint.h>

/* SBI Extension IDs */
#define SBI_EXT_BASE                0x10
#define SBI_EXT_TIME                0x54494D45  /* "TIME" */
#define SBI_EXT_IPI                 0x735049    /* "sPI" */
#define SBI_EXT_RFENCE              0x52464E43  /* "RFNC" */
#define SBI_EXT_HSM                 0x48534D    /* "HSM" - Hart State Management */
#define SBI_EXT_SRST                0x53525354  /* "SRST" - System Reset */

/* Legacy SBI Extension IDs (deprecated but widely supported) */
#define SBI_EXT_LEGACY_SHUTDOWN     0x08

/* SBI System Reset Extension */
#define SBI_SRST_RESET_TYPE_SHUTDOWN    0x00000000
#define SBI_SRST_RESET_TYPE_COLD_REBOOT 0x00000001
#define SBI_SRST_RESET_TYPE_WARM_REBOOT 0x00000002

#define SBI_SRST_RESET_REASON_NONE      0x00000000
#define SBI_SRST_RESET_REASON_SYSFAIL   0x00000001

/* QEMU virt machine test device */
#define QEMU_TEST_DEVICE_ADDR           0x100000
#define QEMU_TEST_DEVICE_EXIT_SUCCESS   0x5555
#define QEMU_TEST_DEVICE_EXIT_FAILURE   0x3333
#define QEMU_TEST_DEVICE_RESET          0x7777

/* SBI Return codes */
#define SBI_SUCCESS                 0
#define SBI_ERR_FAILED              -1
#define SBI_ERR_NOT_SUPPORTED       -2
#define SBI_ERR_INVALID_PARAM       -3
#define SBI_ERR_DENIED              -4
#define SBI_ERR_INVALID_ADDRESS     -5
#define SBI_ERR_ALREADY_AVAILABLE   -6

/**
 * SBI call result
 */
typedef struct {
    long error;
    long value;
} sbi_ret_t;

/**
 * Perform SBI ecall
 */
sbi_ret_t sbi_ecall(long ext, long fid, long arg0, long arg1,
                    long arg2, long arg3, long arg4, long arg5);

/**
 * Shutdown the system
 */
void sbi_shutdown(void);

/**
 * Reboot the system
 */
void sbi_reboot(void);

#endif /* ARCH_SBI_H */
