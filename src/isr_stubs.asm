; isr_stubs.asm  (NASM syntax)
; wrapper for IRQ0 / timer

global timer_isr_asm
global isr0
global irq0
extern timer_isr   ; C function: void timer_isr(cpu_state_t *r)

section .text

timer_isr_asm:
    cli                 ; disable nested interrupts during prologue

    ; push segment registers
    push dword 0x10     ; placeholder for gs (we'll set segments next)
    push dword 0x10     ; fs
    push dword 0x10     ; es
    push dword 0x10     ; ds

    ; save general registers
    pushad              ; pushes EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI (in that order)

    ; If you want to push an interrupt number or error code, do it here.
    ; For IRQs we can push an int number (32 for IRQ0)
    push dword 32       ; int_no (optional)
    push dword 0        ; err_code (0 when none) (optional)

    ; Now call C handler with pointer to the top of this stack frame
    mov eax, esp        ; pointer to cpu_state on stack
    push eax            ; push pointer as argument
    call timer_isr
    add esp, 4

    ; pop the two optional fields (if pushed)
    add esp, 8          ; remove int_no and err_code

    popad               ; restore general-purpose registers
    ; pop segment registers - we pushed 4 dwords, so pop in reverse
    add esp, 16         ; remove the four segment placeholders (we don't need to restore them to previous values, we set DS/ES below if required)

    ; send EOI - do it in C or here; if you do here:
    ; mov al, 0x20
    ; out 0x20, al

    sti                 ; re-enable interrupts (optional; many kernels re-enable with iret)
    iretd               ; return from interrupt

isr0:
    cli
    ; save registers
    pusha
    ; maybe send EOI if it's from PIC, or for exceptions handle
    ; do your handler
    ; restore registers
    popa
    ; End of Interrupt (for IRQs)
    ; iret
    iretd

irq0:
    cli
    pusha
    ; send EOI to PIC
    ; handler code
    popa
    iretd
