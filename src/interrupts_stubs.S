; interrupts_stubs.S
; NASM-style 32-bit stubs for vectors 0..47
; Each stub pushes its vector number and calls the appropriate C handler.

global setGdt
global reloadSegments
global flush_tss

global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

global syscall_handler_asm

extern exception_handler_c   ; void exception_handler_c(unsigned int int_no);
extern irq_handler_c         ; void irq_handler_c(unsigned int int_no);
extern syscall_handler_c

; ------------------------
; Exception stubs 0..31
; ------------------------
%macro isr 1
%define VEC %1
isr%1:
    pusha
    push dword VEC
    call exception_handler_c
    add esp, 4
    popa
    iretd
%endmacro

isr 0
isr 1
isr 2
isr 3
isr 4
isr 5
isr 6
isr 7
isr 8
isr 9
isr 10
isr 11
isr 12
isr 13
isr 14
isr 15
isr 16
isr 17
isr 18
isr 19
isr 20
isr 21
isr 22
isr 23
isr 24
isr 25
isr 26
isr 27
isr 28
isr 29
isr 30
isr 31

; ------------------------
; IRQ stubs 32..47 (irq0..irq15)
; ------------------------
; vector = 32 + IRQ
%macro IRQ_STUB 1
%define IRQ %1
%assign VEC 32 + IRQ
irq%1:
    cli

    pushad
    push ds
    push es
    push fs
    push gs

    push esp
    push dword VEC
    call irq_handler_c
    add esp, 8

    pop gs
    pop fs
    pop es
    pop ds
    popad

    sti
    iretd
%endmacro

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

gdtr DW 0 ; For limit storage
     DD 0 ; For base storage

setGdt:
   MOV   AX, [esp + 4]
   MOV   [gdtr], AX
   MOV   EAX, [ESP + 8]
   MOV   [gdtr + 2], EAX
   LGDT  [gdtr]
   RET

reloadSegments:
   ; Reload CS register containing code selector:
   JMP   0x08:reload_CS ; 0x08 is a stand-in for your code segment
reload_CS:
   ; Reload data segment registers:
   MOV   AX, 0x10 ; 0x10 is a stand-in for your data segment
   MOV   DS, AX
   MOV   ES, AX
   MOV   FS, AX
   MOV   GS, AX
   MOV   SS, AX
   RET

flush_tss:
    mov ax, (5 * 8) | 3
    ltr ax
    ret

syscall_handler_asm:
    cli
    push edx     ; Argument 3
    push ecx     ; Argument 2
    push ebx     ; Argument 1
    push eax     ; Syscall ID

    call syscall_handler_c

    add esp, 16
    sti
    iretd