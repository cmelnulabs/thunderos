/**
 * ThunderOS System Constants
 * 
 * Centralized location for all system-wide magic numbers and constants.
 * This file should be included wherever these constants are needed.
 */

#ifndef KERNEL_CONSTANTS_H
#define KERNEL_CONSTANTS_H

/*
 * ============================================================================
 * FILESYSTEM CONSTANTS
 * ============================================================================
 */

/* Sector size for block devices */
#define SECTOR_SIZE                     512

/* Bits per byte for bitmap operations */
#define BITS_PER_BYTE                   8

/* Maximum path component length */
#define MAX_PATH_COMPONENT_LEN          256

/* Maximum number of path components */
#define MAX_PATH_COMPONENTS             64

/* Maximum argument length for exec */
#define MAX_ARG_LEN                     128

/* Maximum arguments for exec */
#define MAX_EXEC_ARGS                   16

/* Directory entry name maximum length */
#define DIRENT_NAME_MAX                 256

/* ELF maximum program headers */
#define ELF_MAX_PROGRAM_HEADERS         16

/*
 * ============================================================================
 * PROCESS/SCHEDULER CONSTANTS
 * ============================================================================
 */

/* Time slice constants */
#define MICROSECONDS_PER_SECOND         1000000
#define MILLISECONDS_PER_SECOND         1000
#define MICROSECONDS_PER_MILLISECOND    1000

/* Initial stack size (in pages) */
#define INITIAL_STACK_PAGES             8    /* 32KB initial stack */

/* Signal exit code base */
#define SIGNAL_EXIT_BASE                128

/* POSIX wait status: indicates process was stopped (used in waitpid) */
#define WAIT_STOPPED_INDICATOR          0x7F

/* Maximum user synchronization objects */
#define MAX_USER_MUTEXES                64
#define MAX_USER_CONDVARS               64
#define MAX_USER_RWLOCKS                64

/* Process name copy limit (from utsname structure) */
#define UTSNAME_FIELD_LEN               65   /* 64 chars + null */

/*
 * ============================================================================
 * SHELL/UI CONSTANTS
 * ============================================================================
 */

/* Shell buffer sizes */
#define SHELL_CMD_MAX_LEN               256
#define SHELL_MAX_ARGS                  16
#define SHELL_READ_BUFFER_SIZE          256
#define SHELL_PATH_BUFFER_SIZE          256

/* Terminal constants */
#define VTERM_TAB_WIDTH                 8
#define VTERM_MAX_ESCAPE_LEN            7
#define VTERM_CURSOR_HEIGHT             2
#define VTERM_INPUT_BUFFER_SIZE         64

/* Font rendering */
#define FONT_TAB_WIDTH                  4    /* 4-space tabs */

/* ANSI color indices (standard 16-color palette) */
#define ANSI_COLOR_BLACK                0
#define ANSI_COLOR_RED                  1
#define ANSI_COLOR_GREEN                2
#define ANSI_COLOR_YELLOW               3
#define ANSI_COLOR_BLUE                 4
#define ANSI_COLOR_MAGENTA              5
#define ANSI_COLOR_CYAN                 6
#define ANSI_COLOR_WHITE                7
#define ANSI_COLOR_BRIGHT_BLACK         8
#define ANSI_COLOR_BRIGHT_RED           9
#define ANSI_COLOR_BRIGHT_GREEN         10
#define ANSI_COLOR_BRIGHT_YELLOW        11
#define ANSI_COLOR_BRIGHT_BLUE          12
#define ANSI_COLOR_BRIGHT_MAGENTA       13
#define ANSI_COLOR_BRIGHT_CYAN          14
#define ANSI_COLOR_BRIGHT_WHITE         15

/*
 * ============================================================================
 * VIRTIO CONSTANTS
 * ============================================================================
 */

/* VirtIO timeout values (in poll iterations) */
#define VIRTIO_BLK_TIMEOUT_ITERATIONS   1000000
#define VIRTIO_GPU_TIMEOUT_ITERATIONS   1000000

/* VirtIO descriptor flags (if not defined elsewhere) */
#ifndef VIRTQ_DESC_F_NEXT
#define VIRTQ_DESC_F_NEXT               1
#define VIRTQ_DESC_F_WRITE              2
#endif

/*
 * ============================================================================
 * HARDWARE/INTERRUPT CONSTANTS
 * ============================================================================
 */

/* Timer frequency on QEMU virt machine (10 MHz) */
#ifndef TIMER_FREQ_HZ
#define TIMER_FREQ_HZ                   10000000UL
#endif

/* QEMU virt machine MMIO addresses */
#define QEMU_UART0_BASE                 0x10000000UL
#define QEMU_VIRTIO_BASE                0x10001000UL
#define QEMU_VIRTIO_STRIDE              0x1000UL
#define QEMU_VIRTIO_END                 0x10008000UL
#define QEMU_CLINT_BASE                 0x2000000UL
#define QEMU_RAM_START                  0x80000000UL
#define QEMU_RAM_END                    0x88000000UL

/* PLIC constants */
#define PLIC_BITS_PER_WORD              32

/* Timer/interrupt bit positions */
#define STIE_BIT                        5
#define SIE_STIE                        (1 << STIE_BIT)

/* SSTATUS bits */
#define SSTATUS_SPP_BIT                 8

/* Interrupt state for interrupt_restore() */
#define INTERRUPTS_ENABLED              1

/*
 * ============================================================================
 * SYSCALL CONSTANTS
 * ============================================================================
 */

/* Kernel space start address */
#define KERNEL_SPACE_START              0xFFFFFFE000000000UL

/* Standard file descriptors */
#define STDIN_FD                        0
#define STDOUT_FD                       1
#define STDERR_FD                       2

/* Timer tick conversion constants */
#define TIMER_TICK_MS                   100  /* 100ms per tick */
#define TIMER_TICKS_PER_SECOND          (MILLISECONDS_PER_SECOND / TIMER_TICK_MS)

/* Path/name length limits */
#define MAX_PATH_LEN                    4096      /* Maximum path length */
#define MAX_NAME_LEN                    256       /* Maximum file/dir name */
#define MAX_CWD_LEN                     255       /* Maximum current directory path */

/* ext2 filesystem constants */
#define EXT2_MIN_BLOCK_SIZE             1024      /* Minimum block size (1KB) */
#define EXT2_SUPERBLOCK_OFFSET          1024      /* Superblock offset in bytes */
#define EXT2_MODE_MASK                  0xFFF     /* Permission bits mask */

/* ELF loader constants */
#define ELF_PATH_BUFFER_SIZE            256       /* Buffer for ELF path manipulation */
#define ELF_USER_STACK_BASE             0x3FFF0000UL  /* User stack detection base */
#define ELF_USER_STACK_TOP              0x40000000UL  /* User stack detection top */

/* Memory protection flags for mmap */
#define PROT_READ                       0x1
#define PROT_WRITE                      0x2
#define PROT_EXEC                       0x4

/* Mapping flags for mmap */
#define MAP_PRIVATE                     0x02
#define MAP_ANONYMOUS                   0x20

/*
 * ============================================================================
 * UTILITY CONSTANTS
 * ============================================================================
 */

/* String buffer sizes */
#define KPRINT_DEC_MAX_DIGITS           20
#define KPRINT_HEX_BUF_SIZE             19   /* "0x" + 16 hex digits + null */
#define UINT32_MAX_DIGITS               11   /* Max 10 digits + null */

/* Hex formatting */
#define HEX_PREFIX_LEN                  2    /* "0x" */
#define HEX_DIGITS                      16   /* For 64-bit value */
#define HEX_BUF_SIZE                    18   /* "0x" + 16 digits */

/* Framebuffer */
#define FB_BYTES_PER_PIXEL              4    /* 32-bit RGBA */

/*
 * ============================================================================
 * ASCII CONSTANTS
 * ============================================================================
 */

#define ASCII_NUL                       0x00
#define ASCII_BELL                      0x07
#define ASCII_BACKSPACE                 0x08
#define ASCII_TAB                       0x09
#define ASCII_NEWLINE                   0x0A
#define ASCII_RETURN                    0x0D
#define ASCII_ESCAPE                    0x1B
#define ASCII_SPACE                     0x20
#define ASCII_DELETE                    0x7F

#define ASCII_PRINTABLE_MIN             0x20
#define ASCII_PRINTABLE_MAX             0x7F

#endif /* KERNEL_CONSTANTS_H */
