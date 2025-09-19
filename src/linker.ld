ENTRY(kernel_entry)

SECTIONS
{
    . = 0x100000; /* load kernel at 1 MB */

    .multiboot : { *(.multiboot) }
    .text : { *(.text*) }
    .bss : { *(.bss*) }
}