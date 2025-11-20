/*
 * Linker script for ThunderOS kernel
 * RISC-V 64-bit
 * 
 * With -bios none, QEMU starts execution at 0x80000000 in M-mode
 */

OUTPUT_ARCH(riscv)
ENTRY(_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);  /* R + X = 5 */
    data PT_LOAD FLAGS(6);  /* R + W = 6 */
}

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
    } :text
    
    .rodata : {
        PROVIDE(_rodata_start = .);
        *(.rodata .rodata.*)
        PROVIDE(_rodata_end = .);
    } :text
    
    . = ALIGN(4096);  /* Align to page boundary for different permissions */
    
    .data : {
        PROVIDE(_data_start = .);
        *(.data .data.*)
        PROVIDE(_data_end = .);
    } :data
    
    .bss : {
        PROVIDE(_bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE(_bss_end = .);
    } :data
    
    . = ALIGN(4096);
    PROVIDE(_kernel_end = .);
    
    /* Embedded test programs */
    .user_exception_test : {
        user_exception_start = .;
        *(.user_exception_test)
        user_exception_end = .;
    } :data
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
