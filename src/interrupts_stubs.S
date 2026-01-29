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
extern safe_transition_stack

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
    pushad
    push ds
    push es
    push fs
    push gs

    ; 1. Check if CS (at esp + 52) & 0x03 != 3
    mov ax, [esp + 52]
    and ax, 0x03
    cmp ax, 3
    je .skip_jump

    ; 2. Check if EIP (at esp + 48) > 0x500000
    mov eax, [esp + 48]
    cmp eax, 0x500000
    jbe .skip_jump

    .irq_test:
    jmp .skip_jump

    .skip_jump:
    mov ax, [esp + 52]
    and ax, 0x03
    cmp ax, 3
    je .not_from_kernel

    ; Manual Stack Copying using EAX as a buffer
    ; mov eax, [esp + 64]
    mov eax, 0
    mov [safe_transition_stack + 16383], eax
    ; mov eax, [esp + 60]
    mov eax, 0
    mov [safe_transition_stack + 16379], eax
    mov eax, [esp + 56]
    mov [safe_transition_stack + 16375], eax
    mov eax, [esp + 52]
    mov [safe_transition_stack + 16371], eax
    mov eax, [esp + 48]
    mov [safe_transition_stack + 16367], eax
    mov eax, [esp + 44]
    mov [safe_transition_stack + 16363], eax
    mov eax, [esp + 40]
    mov [safe_transition_stack + 16359], eax
    mov eax, [esp + 36]
    mov [safe_transition_stack + 16355], eax
    mov eax, [esp + 32]
    mov [safe_transition_stack + 16351], eax
    mov eax, [esp + 28]
    mov [safe_transition_stack + 16347], eax
    mov eax, [esp + 24]
    mov [safe_transition_stack + 16343], eax
    mov eax, [esp + 20]
    mov [safe_transition_stack + 16339], eax
    mov eax, [esp + 16]
    mov [safe_transition_stack + 16335], eax
    mov eax, [esp + 12]
    mov [safe_transition_stack + 16331], eax
    mov eax, [esp + 8]
    mov [safe_transition_stack + 16327], eax
    mov eax, [esp + 4]
    mov [safe_transition_stack + 16323], eax
    mov eax, [esp]
    mov [safe_transition_stack + 16319], eax

    ; Switch stack pointer
    mov esp, safe_transition_stack + 16319

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    push dword VEC
    call irq_handler_c
    add esp, 8

    ; Save ESP to return later. Put there eip, eflags, cs
    mov eax, [esp+28]
    mov [safe_transition_stack + 16315], eax
    mov ebx, [safe_transition_stack + 16367]
    sub eax, 12
    mov [eax], ebx
    mov ebx, [safe_transition_stack + 16371]
    add eax, 4
    mov [eax], ebx
    mov ebx, [safe_transition_stack + 16375]
    add eax, 4
    mov [eax], ebx

    pop gs
    pop fs
    pop es
    pop ds
    popad

    ; Restore stack pointer
    mov esp, [safe_transition_stack + 16315]
    sub esp, 12
    iretd

.not_from_kernel:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    push dword %1
    call irq_handler_c
    add esp, 8

    pop gs
    pop fs
    pop es
    pop ds
    popad
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
    ;cli

    pushad
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push edx     ; Argument 3
    push ecx     ; Argument 2
    push ebx     ; Argument 1
    push eax     ; Syscall ID

    ;pushfd              ; Push EFLAGS (32-bit) onto the stack
    ;pop eax             ; Pop them into EAX
    ;test eax, 0x200     ; Bit 9 is the Interrupt Flag (0x200)
    ;jnz .interrupts_onb  ; Jump if IF is 1
    ;jz .interrupts_offb ; Jump if IF is 0

    ;.interrupts_onb:
    ;jmp .interrupts_offb

    ;.interrupts_offb:

    call syscall_handler_c

    add esp, 16

    pop gs
    pop fs
    pop es
    pop ds
    popad

    ;sti
    iretd