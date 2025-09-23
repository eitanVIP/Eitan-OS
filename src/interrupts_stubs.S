; interrupts_stubs.S
; NASM-style 32-bit stubs for vectors 0..47
; Each stub pushes its vector number and calls the appropriate C handler.

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

extern exception_handler_c   ; void exception_handler_c(unsigned int int_no);
extern irq_handler_c         ; void irq_handler_c(unsigned int int_no);

; ------------------------
; Exception stubs 0..31
; ------------------------
%macro EXCP_STUB 1
%define VEC %1
isr%1:
    pusha
    push dword VEC
    call exception_handler_c
    add esp, 4
    popa
    iret
%endmacro

EXCP_STUB 0
EXCP_STUB 1
EXCP_STUB 2
EXCP_STUB 3
EXCP_STUB 4
EXCP_STUB 5
EXCP_STUB 6
EXCP_STUB 7
EXCP_STUB 8
EXCP_STUB 9
EXCP_STUB 10
EXCP_STUB 11
EXCP_STUB 12
EXCP_STUB 13
EXCP_STUB 14
EXCP_STUB 15
EXCP_STUB 16
EXCP_STUB 17
EXCP_STUB 18
EXCP_STUB 19
EXCP_STUB 20
EXCP_STUB 21
EXCP_STUB 22
EXCP_STUB 23
EXCP_STUB 24
EXCP_STUB 25
EXCP_STUB 26
EXCP_STUB 27
EXCP_STUB 28
EXCP_STUB 29
EXCP_STUB 30
EXCP_STUB 31

; ------------------------
; IRQ stubs 32..47 (irq0..irq15)
; ------------------------
; vector = 32 + IRQ
%macro IRQ_STUB 1
%define IRQ %1
%assign VEC 32 + IRQ
irq%1:
    pusha
    push dword VEC
    call irq_handler_c
    add esp, 4
    popa
    iret
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
