BITS 32

section .text
    align 4
    dd 0x1BADB002
    dd 0x00000000
    dd -(0x1BADB002 + 0x00000000)

    global kernel_entry      ; entry point for GRUB
    extern kernel_main       ; C kernel entry

    kernel_entry:
        cli                  ; disable interrupts
        mov esp, stack_top   ; set up 32-bit stack
        call kernel_main
        hlt

    halt:
        cli                  ; disable interrupts
        hlt
        jmp halt

section .bss
    resb 16384      ; 16 KB stack
    stack_top: