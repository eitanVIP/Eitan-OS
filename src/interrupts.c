//
// Created by eitan on 9/24/25.
//

#include "interrupts.h"
#include "io.h"
#include "screen.h"
#include "eitan_lib.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193180

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

void exception_handler_c(unsigned int int_no);
void irq_handler_c(unsigned int int_no);

struct GDTR {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));
extern void setGdt(struct GDTR* gdt);
extern void reloadSegments();

extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

typedef struct {
    unsigned short    isr_low;      // The lower 16 bits of the ISR's address
    unsigned short    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
    unsigned char     reserved;     // Set to zero
    unsigned char     attributes;   // Type and attributes; see the IDT page
    unsigned short    isr_high;     // The higher 16 bits of the ISR's address
} __attribute__((packed)) idt_entry_t;

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct GDT {
    unsigned int base;        // 32-bit base
    unsigned int limit;       // 20-bit limit (stored in 32-bit int)
    unsigned char access_byte;  // Access byte
    unsigned char flags;        // Only 4 bits used, but stored in 8-bit int
};

unsigned char gdt[5 * 8];

__attribute__((aligned(0x10)))
static idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance
struct idt_ptr idtp;

void idt_set_descriptor(unsigned char vector, void* isr, unsigned char flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (unsigned int)isr & 0xFFFF;
    descriptor->kernel_cs      = 0x08; // this value can be whatever offset your kernel code selector is in your GDT
    descriptor->attributes     = flags;
    descriptor->isr_high       = (unsigned int)isr >> 16;
    descriptor->reserved       = 0;
}

static inline void lidt(void* base, unsigned short size) {
    struct idt_ptr IDTR = { size, (unsigned int) base };
    asm volatile("lidt %0" : : "m"(IDTR));
}

void encode_gdt_entry(unsigned char *target, struct GDT source)
{
    // Check the limit to make sure that it can be encoded
    if (source.limit > 0xFFFFF) {
        screen_print("GDT cannot encode limits larger than 0xFFFFF\n", 45);
        return;
    }

    // Encode the limit
    target[0] = source.limit & 0xFF;
    target[1] = (source.limit >> 8) & 0xFF;
    target[6] = (source.limit >> 16) & 0x0F;

    // Encode the base
    target[2] = source.base & 0xFF;
    target[3] = (source.base >> 8) & 0xFF;
    target[4] = (source.base >> 16) & 0xFF;
    target[7] = (source.base >> 24) & 0xFF;

    // Encode the access byte
    target[5] = source.access_byte;

    // Encode the flags
    target[6] |= (source.flags << 4);
}

void interrupts_init() {
    // GDT
    encode_gdt_entry(gdt, (struct GDT){ 0, 0, 0, 0 });
    encode_gdt_entry(gdt + 0x0008, (struct GDT){ 0, 0xFFFFF, 0x9A, 0xC });
    encode_gdt_entry(gdt + 0x0010, (struct GDT){ 0, 0xFFFFF, 0x92, 0xC });
    encode_gdt_entry(gdt + 0x0018, (struct GDT){ 0, 0xFFFFF, 0xFA, 0xC });
    encode_gdt_entry(gdt + 0x0020, (struct GDT){ 0, 0xFFFFF, 0xF2, 0xC });
    struct GDTR gdtr;
    gdtr.limit = 5 * 8 - 1;
    gdtr.base = (unsigned int) gdt;
    setGdt(&gdtr);
    reloadSegments();

    // IDT
    for (int i = 0; i < 256; ++i) {
        idt_set_descriptor((unsigned char)i, isr0, 0x8E);
    }

    // /* Exceptions 0..31 */
    // idt_set_gate(0,  isr0,  0x08, 0x8E);
    // idt_set_gate(1,  isr1,  0x08, 0x8E);
    // idt_set_gate(2,  isr2,  0x08, 0x8E);
    // idt_set_gate(3,  isr3,  0x08, 0x8E);
    // idt_set_gate(4,  isr4,  0x08, 0x8E);
    // idt_set_gate(5,  isr5,  0x08, 0x8E);
    // idt_set_gate(6,  isr6,  0x08, 0x8E);
    // idt_set_gate(7,  isr7,  0x08, 0x8E);
    // idt_set_gate(8,  isr8,  0x08, 0x8E);
    // idt_set_gate(9,  isr9,  0x08, 0x8E);
    // idt_set_gate(10, isr10, 0x08, 0x8E);
    // idt_set_gate(11, isr11, 0x08, 0x8E);
    // idt_set_gate(12, isr12, 0x08, 0x8E);
    // idt_set_gate(13, isr13, 0x08, 0x8E);
    // idt_set_gate(14, isr14, 0x08, 0x8E);
    // idt_set_gate(15, isr15, 0x08, 0x8E);
    // idt_set_gate(16, isr16, 0x08, 0x8E);
    // idt_set_gate(17, isr17, 0x08, 0x8E);
    // idt_set_gate(18, isr18, 0x08, 0x8E);
    // idt_set_gate(19, isr19, 0x08, 0x8E);
    // idt_set_gate(20, isr20, 0x08, 0x8E);
    // idt_set_gate(21, isr21, 0x08, 0x8E);
    // idt_set_gate(22, isr22, 0x08, 0x8E);
    // idt_set_gate(23, isr23, 0x08, 0x8E);
    // idt_set_gate(24, isr24, 0x08, 0x8E);
    // idt_set_gate(25, isr25, 0x08, 0x8E);
    // idt_set_gate(26, isr26, 0x08, 0x8E);
    // idt_set_gate(27, isr27, 0x08, 0x8E);
    // idt_set_gate(28, isr28, 0x08, 0x8E);
    // idt_set_gate(29, isr29, 0x08, 0x8E);
    // idt_set_gate(30, isr30, 0x08, 0x8E);
    // idt_set_gate(31, isr31, 0x08, 0x8E);
    //
    // /* IRQs 32..47 (irq0..irq15) */
    // idt_set_gate(32, irq0,  0x08, 0x8E);
    // idt_set_gate(33, irq1,  0x08, 0x8E);
    // idt_set_gate(34, irq2,  0x08, 0x8E);
    // idt_set_gate(35, irq3,  0x08, 0x8E);
    // idt_set_gate(36, irq4,  0x08, 0x8E);
    // idt_set_gate(37, irq5,  0x08, 0x8E);
    // idt_set_gate(38, irq6,  0x08, 0x8E);
    // idt_set_gate(39, irq7,  0x08, 0x8E);
    //
    // idt_set_gate(40, irq8,  0x08, 0x8E);
    // idt_set_gate(41, irq9,  0x08, 0x8E);
    // idt_set_gate(42, irq10, 0x08, 0x8E);
    // idt_set_gate(43, irq11, 0x08, 0x8E);
    // idt_set_gate(44, irq12, 0x08, 0x8E);
    // idt_set_gate(45, irq13, 0x08, 0x8E);
    // idt_set_gate(46, irq14, 0x08, 0x8E);
    // idt_set_gate(47, irq15, 0x08, 0x8E);

    lidt(idt, sizeof(idt) - 1);

    // PIC Remap
    unsigned char a1, a2;
    // Save masks
    a1 = io_inb(0x21);
    a2 = io_inb(0xA1);
    // Starts the initialization sequence (in cascade mode)
    io_outb(0x20, 0x11);
    io_outb(0xA0, 0x11);
    // Set vector offsets
    io_outb(0x21, 0x20); // Master PIC vector offset: 32-39
    io_outb(0xA1, 0x28); // Slave PIC vector offset: 40-47
    // Tell Master PIC about the Slave PIC at IRQ2 (0000 0100)
    io_outb(0x21, 4);
    // Tell Slave PIC its cascade identity (0000 0010)
    io_outb(0xA1, 2);
    // Set PICs to 8086/88 mode
    io_outb(0x21, 0x01);
    io_outb(0xA1, 0x01);
    // Restore saved masks
    io_outb(0x21, a1);
    io_outb(0xA1, a2);

    // unsigned char mask = io_inb(0x21); // master PIC mask port
    // char buf[32];
    // /* convert mask to hex string quickly */
    // buf[0] = 'M'; buf[1] = 'K'; buf[2] = ':'; buf[3] = ' ';
    // buf[4] = "0123456789ABCDEF"[(mask >> 4) & 0xF];
    // buf[5] = "0123456789ABCDEF"[mask & 0xF];
    // buf[6] = 0;
    // screen_print(buf, 6);

    // Unmask IRQ0 on master PIC so PIT ticks get through
    {
        unsigned char mask = io_inb(PIC1_DATA);
        mask &= ~(1 << 0); /* clear bit 0 */
        io_outb(PIC1_DATA, mask);
    }

    // PIT
    unsigned short divisor = PIT_FREQ / 100; // 100Hz
    // 0b00110110: channel 0, low/high byte, mode 3 (square wave)
    io_outb(PIT_COMMAND, 0b00110110);
    io_outb(PIT_CHANNEL0, divisor & 0xFF);       // low byte
    io_outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);// high byte

    asm volatile("int $0x20");
    // Enable interrupts
    asm volatile("sti");
}

void send_eoi(unsigned int irq_number) {
    if (irq_number >= 8) {
        // If IRQ came from Slave PIC
        io_outb(PIC2_COMMAND, PIC_EOI); // Send EOI to Slave PIC
    }
    // Always send EOI to Master PIC
    io_outb(PIC1_COMMAND, PIC_EOI); // Send EOI to Master PIC
}

void exception_handler_c(unsigned int int_no) {
    char* msg = str_concat("CPU Exception: ", num_to_str(int_no));
    screen_print(msg, strlen(msg));

    while (1) {
        asm volatile("hlt");
    }
}

void irq_handler_c(unsigned int int_no) {
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);
    screen_print("Shit", 4);

    if (int_no < 32 || int_no > 47) return;

    unsigned int irq = int_no - 32;

    if (irq == 0) {
        // Timer tick: schedule next process
    } else if (irq == 1) {
        // Keyboard shit
    } else {
        // other IRQs
    }

    // Acknowledge PIC(s)
    send_eoi(irq);
}