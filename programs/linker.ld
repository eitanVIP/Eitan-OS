ENTRY(_start)

SECTIONS
{
    . = 0x2000000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}