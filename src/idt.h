//
// Created by eitan on 9/23/25.
//

#pragma once

#define IDT_ENTRIES 256

// IDT entry (32-bit)
struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;     // code segment selector in GDT
    unsigned char  zero;
    unsigned char  flags;
    unsigned short offset_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

extern struct idt_entry idt[IDT_ENTRIES];
extern struct idt_ptr   idtp;

void idt_set_gate(unsigned char vector, unsigned int isr_addr, unsigned short sel, unsigned char flags);
void idt_install(void);