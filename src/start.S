; start.S - NASM syntax with Multiboot2 header
BITS 64

; ========================
; Multiboot2 Header (required by GRUB)
; ========================
section .multiboot
align 8

MB2_MAGIC      equ 0xE85250D6
MB2_ARCH_X86_64 equ 0x00000002
MB2_HEADER_LEN equ 24

dd MB2_MAGIC               ; magic number
dd MB2_ARCH_X86_64         ; architecture (x86_64)
dd MB2_HEADER_LEN          ; header length
dd -(MB2_MAGIC + MB2_ARCH_X86_64 + MB2_HEADER_LEN) ; checksum
dq 0                       ; reserved / padding

; ========================
; Stack section
; ========================
section .bss
align 16
stack:
    resb 16384      ; 16 KB stack
stack_top:

; ========================
; Code section
; ========================
section .text
global kernel_entry      ; entry point for GRUB
extern kernel_main       ; C kernel entry

kernel_entry:
    mov rsp, stack_top   ; set up 64-bit stack
    call kernel_main

halt:
    cli                  ; disable interrupts
.hlt_loop:
    hlt
    jmp .hlt_loop
