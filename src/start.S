BITS 32

section .text
    align 4
    dd 0x1BADB002
    dd 0x00000000
    dd -(0x1BADB002 + 0x00000000)

    global kernel_entry      ; entry point for GRUB
    extern kernel_main       ; C kernel entry

    kernel_entry:
        cli                  ; disable interrupts
        mov esp, stack_top   ; set up 32-bit stack
        push ebx             ; pointer to multiboot data

        ; minimal page table setup for 64 bit switch
        mov eax, pdpt
        or  eax, 0x3        ; present + writable
        mov [pml4], eax

        mov eax, pd
        or  eax, 0x3
        mov [pdpt], eax

        ; First PD entry: 2MB huge page at physical 0x0
        ; Bit 7 = huge page flag
        mov dword [pd], 0x83   ; present + writable + huge

    enable_long_mode:
        ; Load CR3 with PML4
        mov eax, pml4
        mov cr3, eax

        ; Enable PAE (bit 5 of CR4)
        mov eax, cr4
        or  eax, (1 << 5)
        mov cr4, eax

        ; Set EFER.LME (bit 8) via MSR 0xC0000080
        mov ecx, 0xC0000080     ; EFER MSR number
        rdmsr                   ; read into edx:eax
        or  eax, (1 << 8)       ; set LME
        wrmsr

        ; Enable paging + protected mode in CR0
        ; Paging (bit 31) + Write Protect (bit 16) + Protected Mode (bit 0)
        mov eax, cr0
        or  eax, (1 << 31) | (1 << 0)
        mov cr0, eax
        ; ← long mode is now ACTIVE but we're still in a 32-bit segment!

        ; Load the new 64-bit GDT
        lgdt [gdt64.pointer]

        ; Far jump to flush the pipeline and land in a 64-bit segment
        jmp CODE64_SEG:long_mode_entry

BITS 64
    long_mode_entry:
        mov rsp, stack_top
        sub rsp, 4

        ; restore multiboot pointer
        pop rdi

        extern kernel_main
        call kernel_main

    halt:
        cli                  ; disable interrupts
        hlt
        jmp halt

section .data
    align 8
    gdt64:
        dq 0x0000000000000000   ; null descriptor

    .code_descriptor:
        ; Flags: present, DPL=0, code, 64-bit (L=1)
        dq 0x00AF9A000000FFFF

    .data_descriptor:
        ; Flags: present, DPL=0, data, writable
        dq 0x00CF92000000FFFF

    .pointer:
        dw $ - gdt64 - 1        ; limit (size - 1)
        dq gdt64                ; base address

    CODE64_SEG equ 0x08         ; offset of code descriptor
    DATA64_SEG equ 0x10         ; offset of data descriptor

section .bss
    resb 16384      ; 16 KB stack
    stack_top:

    align 4096
    pml4:  resb 4096
    pdpt:  resb 4096
    pd:    resb 4096
    ; no PT needed — we use a 2MB huge page
