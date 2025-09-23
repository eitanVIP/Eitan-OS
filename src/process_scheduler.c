//
// Created by eitan on 9/22/25.
//

#include "process_scheduler.h"
#include "io.h"
#include "idt.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193180  // PIT base frequency
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

extern void timer_isr_asm();

process_t* current_process;

void pit_init(unsigned int hz) {
    unsigned short divisor = PIT_FREQ / hz;
    // 0x36: channel 0, low/high byte, mode 3 (square wave)
    io_outb(PIT_COMMAND, 0x36);
    io_outb(PIT_CHANNEL0, divisor & 0xFF);       // low byte
    io_outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);// high byte

    idt_set_gate(32, (unsigned int)timer_isr_asm, 0x08, 0x8E); // IRQ0

    asm volatile("sti"); // enable CPU interrupts
}

void pic_remap(int offset1, int offset2) {
    unsigned char a1 = io_inb(PIC1_DATA);
    unsigned char a2 = io_inb(PIC2_DATA);

    io_outb(PIC1_COMMAND, ICW1_INIT);
    io_outb(PIC2_COMMAND, ICW1_INIT);
    io_outb(PIC1_DATA, offset1);
    io_outb(PIC2_DATA, offset2);
    io_outb(PIC1_DATA, 4);    // tell master about slave at IRQ2
    io_outb(PIC2_DATA, 2);    // tell slave its cascade identity
    io_outb(PIC1_DATA, ICW4_8086);
    io_outb(PIC2_DATA, ICW4_8086);

    io_outb(PIC1_DATA, a1);
    io_outb(PIC2_DATA, a2);
}

void schedule() {

}

void timer_isr(cpu_state_t* r) {
    if (!current_process) {
        // no process to save/restore — maybe run idle task or just return
        io_outb(0x20, 0x20); // EOI
        return;
    }

    // Save general-purpose registers from struct into process->regs
    current_process->regs.eax = r->eax;
    current_process->regs.ebx = r->ebx;
    current_process->regs.ecx = r->ecx;
    current_process->regs.edx = r->edx;
    current_process->regs.esi = r->esi;
    current_process->regs.edi = r->edi;
    current_process->regs.ebp = r->ebp;
    current_process->regs.esp = r->esp;  // NOTE: this is original esp value saved by pushad
    current_process->regs.eip = r->eip;
    current_process->regs.eflags = r->eflags;

    // scheduling decision
    schedule();

    // Load next process regs into r so that the wrapper pop/iret returns to new process
    r->eax = current_process->regs.eax;
    r->ebx = current_process->regs.ebx;
    r->ecx = current_process->regs.ecx;
    r->edx = current_process->regs.edx;
    r->esi = current_process->regs.esi;
    r->edi = current_process->regs.edi;
    r->ebp = current_process->regs.ebp;
    r->esp = current_process->regs.esp;
    r->eip = current_process->regs.eip;
    r->eflags = current_process->regs.eflags;

    // Send EOI to PIC (master). If IRQ >= 8, also send to slave 0xA0.
    io_outb(0x20, 0x20);
}