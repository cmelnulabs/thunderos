/* Linker script for ThunderOS userland programs */

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* User programs start at 0xf000 (64KB offset) */
    . = 0xf000;
    
    .text : {
        *(.text._start)
        *(.text .text.*)
    }
    
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }
    
    .data : {
        __data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        __data_end = .;
    }
    
    .bss : ALIGN(8) {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    _end = .;
    
    /* Set global pointer for gp-relative addressing
     * Standard convention: gp points to .sdata + 0x800 
     * This allows gp-relative access to ±2KB range */
    __global_pointer$ = __data_start + 0x800;
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
