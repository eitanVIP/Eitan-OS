[BITS 32]

section .data
    msg db "Hallo Ligma", 0xA, 0   ; 0xA is a newline (\n)
    msg_len equ $ - msg            ; This calculates the length automatically

section .text
global _start

_start:
    mov eax, 30
    mov ebx, msg
    int 0x80

    mov ecx, 0
.loop:
    add ecx, 1
    cmp ecx, 1000000000
    jl .loop
    jmp _start
    ;jmp .loop