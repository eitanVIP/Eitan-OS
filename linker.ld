OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_entry)

SECTIONS
{
    . = 0x100000; /* load kernel at 1 MB */
    .text : { *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss) }
}