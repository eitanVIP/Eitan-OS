section .text
global _start
extern main

_start:
    call main

    mov eax, 0
    int 0x80