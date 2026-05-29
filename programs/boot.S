section .text
global _start
extern main

_start:
    call main

    mov rax, 0
    int 0x80