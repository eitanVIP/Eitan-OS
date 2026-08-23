ENTRY(_start)

SECTIONS
{
    . = 0x2100000;

    .text : {
        *(.text)
    }

    . = ALIGN(0x1000);

    .data : {
        *(.data)
    }

    . = ALIGN(0x1000);

    .bss : {
        *(.bss)
    }
}