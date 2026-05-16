bits 64

global setGdt
global reloadSegments
global flush_tss
global syscall_handler_asm
global enable_sse
global enable_nxe

global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

global irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
global irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15

extern exception_handler_c
extern irq_handler_c
extern syscall_handler_c

; ----- Stack layout after PUSH_ALL ----------
;
;  [rsp +   0]  r15     <- PUSH_ALL starts here
;  [rsp +   8]  r14
;  [rsp +  16]  r13
;  [rsp +  24]  r12
;  [rsp +  32]  r11
;  [rsp +  40]  r10
;  [rsp +  48]  r9
;  [rsp +  56]  r8
;  [rsp +  64]  rbp
;  [rsp +  72]  rdi
;  [rsp +  80]  rsi
;  [rsp +  88]  rdx
;  [rsp +  96]  rcx
;  [rsp + 104]  rbx
;  [rsp + 112]  rax
;  [rsp + 120]  error_code   (pushed by CPU or dummy 0)
;  ----- CPU-pushed interrupt frame (always present in 64-bit mode) ----------
;  [rsp + 128]  rip
;  [rsp + 136]  cs
;  [rsp + 144]  rflags
;  [rsp + 152]  rsp  (before interrupt)
;  [rsp + 160]  ss

%define FRAME_CS 136    ; offset of cs from rsp after PUSH_ALL

; ----- Macros ----------

%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push fs
    push gs
%endmacro

%macro POP_ALL 0
    pop gs
    pop fs
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; Load kernel data segments (DS/ES mostly ignored in 64-bit but good practice)
%macro LOAD_KERNEL_SEGS 0
    mov ax, 0x10
    mov ds, ax
    mov es, ax
%endmacro

; ----- Exception stubs (vectors 0–31) ----------
; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
; All others get a dummy 0 pushed so the stack layout is always identical.

%macro ISR_NOERRCODE 1
isr%1:
    push qword 0        ; dummy error code — keeps cpu_regs_t layout uniform
    PUSH_ALL
    LOAD_KERNEL_SEGS
    mov rdi, %1         ; arg1: vector number
    mov rsi, rsp        ; arg2: cpu_regs_t *
    call exception_handler_c
    POP_ALL
    add rsp, 8          ; pop error code
    iretq
%endmacro

%macro ISR_ERRCODE 1
isr%1:
                        ; CPU already pushed the error code
    PUSH_ALL
    LOAD_KERNEL_SEGS
    mov rdi, %1
    mov rsi, rsp
    call exception_handler_c
    POP_ALL
    add rsp, 8          ; pop error code
    iretq
%endmacro

ISR_NOERRCODE 0     ; #DE  divide by zero
ISR_NOERRCODE 1     ; #DB  debug
ISR_NOERRCODE 2     ;      NMI
ISR_NOERRCODE 3     ; #BP  breakpoint
ISR_NOERRCODE 4     ; #OF  overflow
ISR_NOERRCODE 5     ; #BR  bound range exceeded
ISR_NOERRCODE 6     ; #UD  invalid opcode
ISR_NOERRCODE 7     ; #NM  device not available
ISR_ERRCODE   8     ; #DF  double fault
ISR_NOERRCODE 9     ;      coprocessor segment overrun (legacy)
ISR_ERRCODE   10    ; #TS  invalid TSS
ISR_ERRCODE   11    ; #NP  segment not present
ISR_ERRCODE   12    ; #SS  stack-segment fault
ISR_ERRCODE   13    ; #GP  general protection fault
ISR_ERRCODE   14    ; #PF  page fault  ← you'll need this for VMM
ISR_NOERRCODE 15    ;      reserved
ISR_NOERRCODE 16    ; #MF  x87 floating-point
ISR_ERRCODE   17    ; #AC  alignment check
ISR_NOERRCODE 18    ; #MC  machine check
ISR_NOERRCODE 19    ; #XM  SIMD floating-point
ISR_NOERRCODE 20    ; #VE  virtualization
ISR_ERRCODE   21    ; #CP  control protection
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29    ; #VC  VMM communication
ISR_ERRCODE   30    ; #SX  security exception
ISR_NOERRCODE 31

; ----- IRQ stubs (vectors 32+) (Context Switching) ----------

%macro IRQ_STUB 1
%assign VEC (32 + %1)
irq%1:
    push qword 0        ; no error code
    PUSH_ALL
    LOAD_KERNEL_SEGS

    ; If interrupted user-mode code, swap GS to get kernel GS base
    test byte [rsp + FRAME_CS], 3
    jz .call_handler
    swapgs

.call_handler:
    mov rdi, VEC        ; arg1: vector number
    mov rsi, rsp        ; arg2: cpu_regs_t *  (scheduler may modify this)
    call irq_handler_c

    ; Check where we're returning to — may differ from where we came from
    ; if the scheduler switched tasks and changed regs->cs
    test byte [rsp + FRAME_CS], 3
    jz .done
    swapgs              ; swap back to user GS base

.done:
    POP_ALL
    add rsp, 8          ; pop error code slot
    iretq               ; restores rip, cs, rflags, rsp, ss from frame
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

syscall_handler_asm:
    push qword 0
    PUSH_ALL
    LOAD_KERNEL_SEGS

    test byte [rsp + FRAME_CS], 3
    jz .call_handler
    swapgs

.call_handler:
    mov rdi, rsp        ; only arg: cpu_regs_t *
    call syscall_handler_c

    test byte [rsp + FRAME_CS], 3
    jz .done
    swapgs

.done:
    POP_ALL
    add rsp, 8
    iretq

; ----- GDT helpers ----------

section .data
align 8
gdtr:
    dw 0    ; limit (2 bytes)
    dq 0    ; base  (8 bytes)

section .text
setGdt:
    mov [gdtr],     di
    mov [gdtr + 2], rsi
    lgdt [gdtr]
    ret

reloadSegments:
    push rbp
    mov rbp, rsp
    and rsp, -16
    push 0x08
    lea  rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    mov   ax, 0x10
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    mov   ss, ax
    mov   rsp, rbp
    pop   rbp
    ret

flush_tss:
    mov ax, 0x28
    ltr ax
    ret

; ----- Enable SSE ----------

enable_sse:
    ; --- Step 1: Configure CR0 ---
    mov rax, cr0
    ; Clear the EM bit (bit 2) - we don't want to emulate the FPU
    ; Set the MP bit (bit 1) - monitor co-processor
    and ax, 0xFFFB      ; Clear bit 2
    or ax, 0x2          ; Set bit 1
    mov cr0, rax

    ; --- Step 2: Configure CR4 ---
    mov rax, cr4
    ; Set OSFXSR (bit 9) - tells the OS to support FXSAVE/FXRSTOR
    ; Set OSXMMEXCPT (bit 10) - tells the OS to handle SIMD exceptions
    or ax, (3 << 9)     ; Sets both bits 9 and 10
    mov cr4, rax

    ret

; ----- Enable NXE (no-execute memory pages) ----------

enable_nxe:
    mov ecx, 0xC0000080    ; Load the EFER MSR address into ECX
    rdmsr                  ; Read EFER. Low 32 bits go to EAX, high 32 bits to EDX

    or eax, (1 << 11)      ; OR the low 32 bits (EAX) with the NXE bit mask (bit 11)

    wrmsr                  ; Write the updated EAX and EDX back to the EFER MSR
    ret