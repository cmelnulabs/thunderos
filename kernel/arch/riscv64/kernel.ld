/*
 * Linker script for ThunderOS kernel
 * RISC-V 64-bit
 * 
 * With -bios none, QEMU starts execution at 0x80000000 in M-mode
 */

OUTPUT_ARCH(riscv)
ENTRY(_entry)

SECTIONS
{
    /* Kernel starts at 0x80000000 (QEMU -bios none entry point) */
    . = 0x80000000;
    
    .text : {
        PROVIDE(_text_start = .);
        *(.text.entry)     /* M-mode entry from entry.S */
        *(.text.boot)      /* S-mode boot from boot.S */
        *(.text .text.*)
        PROVIDE(_text_end = .);
    }
    
    .rodata : {
        PROVIDE(_rodata_start = .);
        *(.rodata .rodata.*)
        PROVIDE(_rodata_end = .);
    }
    
    .data : {
        PROVIDE(_data_start = .);
        *(.data .data.*)
        PROVIDE(_data_end = .);
    }
    
    .bss : {
        PROVIDE(_bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE(_bss_end = .);
    }
    
    . = ALIGN(4096);
    PROVIDE(_kernel_end = .);
    
    /* Embedded test programs */
    .user_exception_test : {
        user_exception_start = .;
        *(.user_exception_test)
        user_exception_end = .;
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
