[BITS 32]
section .text
global _start

_start:
    mov eax, 0              ; Counter starts at 0
.loop:
    add eax, 1              ; Increment
    cmp eax, 1000000000     ; Compare to 1 Billion
    jl .loop                ; If less, loop again

    ; Once done, we have a problem: no exit() syscall yet!
    ; For now, just halt so the CPU doesn't run into garbage memory.
    hlt