.global _start
_start:
    call main

    mov eax, 0
    int 0x80