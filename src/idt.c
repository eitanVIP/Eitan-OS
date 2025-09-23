//
// Created by eitan on 9/23/25.
//

#include "idt.h"

// define the IDT array and pointer
struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr   idtp;

// external ISR functions (implemented elsewhere; stubs or wrappers)
extern void isr0();  // e.g. divide by zero
// extern void isr1();
// … up to however many ISRs you define (exceptions, timer, etc.)

void idt_set_gate(unsigned char vector, unsigned int isr_addr, unsigned short sel, unsigned char flags) {
    idt[vector].offset_low = isr_addr & 0xFFFF;
    idt[vector].selector   = sel;
    idt[vector].zero       = 0;
    idt[vector].flags      = flags;
    idt[vector].offset_high = (isr_addr >> 16) & 0xFFFF;
}

void idt_install(void) {
    idtp.base = (unsigned int)&idt;
    idtp.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;

    // Clear IDT to start
    for(int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set ISRs you care about, e.g. exception 0:
    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);
    // idt_set_gate(1, (unsigned int)isr1, 0x08, 0x8E);
    // timer IRQ (after remapping PIC) maybe vector 32:
    extern void irq0();  // timer
    idt_set_gate(32, (unsigned int)irq0, 0x08, 0x8E);

    // etc.

    // Load the IDT
    asm volatile("lidt %0" : : "m"(idtp));

    // Enable interrupts
    asm volatile("sti");
}
