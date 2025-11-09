/*
 * Linker script for ThunderOS kernel
 * RISC-V 64-bit
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 0x80200000 (OpenSBI loads here) */
    . = 0x80200000;
    
    .text : {
        PROVIDE(_text_start = .);
        *(.text.boot)
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
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
