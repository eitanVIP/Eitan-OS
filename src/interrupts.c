//
// Created by eitan on 9/24/25.
//

#include "interrupts.h"
#include "io.h"
#include "screen.h"
#include "eitan_lib.h"
#include "filesystem.h"
#include "process_scheduler.h"
#include "memory.h"
#include "program_loader.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193180

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

void exception_handler_c(unsigned int int_no);
void irq_handler_c(unsigned int int_no, unsigned int* regs);
void syscall_handler_c(unsigned int syscall_id, unsigned int arg1, unsigned int arg2, unsigned int arg3);

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

extern void syscall_handler_asm();

typedef struct {
    unsigned short    isr_low;      // The lower 16 bits of the ISR's address
    unsigned short    kernel_cs;    // Offset to index in GDT
    unsigned char     reserved;     // Set to zero
    unsigned char     attributes;   // Type and attributes; see the IDT page
    unsigned short    isr_high;     // The higher 16 bits of the ISR's address
} __attribute__((packed)) idt_entry_t;

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

__attribute__((aligned(0x10)))
static idt_entry_t idt[256];

void idt_set_descriptor(unsigned char vector, void* isr, unsigned char flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (unsigned int)isr & 0xFFFF;
    descriptor->kernel_cs      = 0x08; // Offset to index in GDT, kernel code entry
    descriptor->attributes     = flags;
    descriptor->isr_high       = (unsigned int)isr >> 16;
    descriptor->reserved       = 0;
}

static inline void lidt(void* base, unsigned short size) {
    struct idt_ptr IDTR = { size, (unsigned int) base };
    asm volatile("lidt %0" : : "m"(IDTR));
}

void interrupts_init() {
    // IDT
    for (int i = 0; i < 256; ++i) {
        idt_set_descriptor((unsigned char)i, isr0, 0x8E);
    }

    /* Exceptions 0..31 */
    idt_set_descriptor(0,  isr0,  0x8E);
    idt_set_descriptor(1,  isr1,  0x8E);
    idt_set_descriptor(2,  isr2,  0x8E);
    idt_set_descriptor(3,  isr3,  0x8E);
    idt_set_descriptor(4,  isr4,  0x8E);
    idt_set_descriptor(5,  isr5,  0x8E);
    idt_set_descriptor(6,  isr6,  0x8E);
    idt_set_descriptor(7,  isr7,  0x8E);
    idt_set_descriptor(8,  isr8,  0x8E);
    idt_set_descriptor(9,  isr9,  0x8E);
    idt_set_descriptor(10, isr10, 0x8E);
    idt_set_descriptor(11, isr11, 0x8E);
    idt_set_descriptor(12, isr12, 0x8E);
    idt_set_descriptor(13, isr13, 0x8E);
    idt_set_descriptor(14, isr14, 0x8E);
    idt_set_descriptor(15, isr15, 0x8E);
    idt_set_descriptor(16, isr16, 0x8E);
    idt_set_descriptor(17, isr17, 0x8E);
    idt_set_descriptor(18, isr18, 0x8E);
    idt_set_descriptor(19, isr19, 0x8E);
    idt_set_descriptor(20, isr20, 0x8E);
    idt_set_descriptor(21, isr21, 0x8E);
    idt_set_descriptor(22, isr22, 0x8E);
    idt_set_descriptor(23, isr23, 0x8E);
    idt_set_descriptor(24, isr24, 0x8E);
    idt_set_descriptor(25, isr25, 0x8E);
    idt_set_descriptor(26, isr26, 0x8E);
    idt_set_descriptor(27, isr27, 0x8E);
    idt_set_descriptor(28, isr28, 0x8E);
    idt_set_descriptor(29, isr29, 0x8E);
    idt_set_descriptor(30, isr30, 0x8E);
    idt_set_descriptor(31, isr31, 0x8E);
    
    /* IRQs 32..47 (irq0..irq15) */
    idt_set_descriptor(32, irq0,  0x8E);
    idt_set_descriptor(33, irq1,  0x8E);
    idt_set_descriptor(34, irq2,  0x8E);
    idt_set_descriptor(35, irq3,  0x8E);
    idt_set_descriptor(36, irq4,  0x8E);
    idt_set_descriptor(37, irq5,  0x8E);
    idt_set_descriptor(38, irq6,  0x8E);
    idt_set_descriptor(39, irq7,  0x8E);
    
    idt_set_descriptor(40, irq8,  0x8E);
    idt_set_descriptor(41, irq9,  0x8E);
    idt_set_descriptor(42, irq10, 0x8E);
    idt_set_descriptor(43, irq11, 0x8E);
    idt_set_descriptor(44, irq12, 0x8E);
    idt_set_descriptor(45, irq13, 0x8E);
    idt_set_descriptor(46, irq14, 0x8E);
    idt_set_descriptor(47, irq15, 0x8E);

    idt_set_descriptor(0x80, syscall_handler_asm, 0xEE);

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
    screen_print(msg);

    while (1) {
        asm volatile("hlt");
    }
}

void irq_handler_c(unsigned int int_no, unsigned int* regs) {
    if (int_no < 32 || int_no > 47) return;

    unsigned int irq = int_no - 32;

    if (irq == 0) {
        process_scheduler_next_process(regs);
    } else if (irq == 1) {
        // Keyboard shit
    } else {
        // Other IRQs
    }

    send_eoi(irq);
}

void syscall_handler_c(unsigned int syscall_id, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    switch (syscall_id) {
        case 0: // Close process
            break;

        case 1: // Run program (arg1 pointer to name of file)
            uint8_t* data1;
            uint32_t data_size1;
            if (filesystem_read_file((const char*)arg1, &data1, &data_size1)) {
                program_loader_load_elf32(data1);
            } else {
                screen_println("Couldn't load file");
            }
            break;



        case 10: // Malloc (arg1 size)
            malloc(arg1);
            break;

        case 11: // Free (arg1 pointer)
            free((void*)arg1);
            break;



        case 20: // Read keyboard char (arg1 pointer to put char)
            *(uint16_t*)arg1 = io_keyboard_read();
            break;



        case 30: // Print string (arg1 pointer to string)
            screen_print((const char*)arg1);
            break;

        case 31: // Clear screen
            screen_clear_screen();
            break;



        case 40: // Read file (arg1 pointer to name of file, arg2 pointer to put pointer to heap data, arg3 size of data)
            uint8_t* data2;
            uint32_t data_size2;
            if (filesystem_read_file((const char*)arg1, &data2, &data_size2)) {
                *(uint8_t**)arg2 = data2;
                *(uint8_t*)arg3 = data_size2;
            } else {
                screen_println("Couldn't load file");
            }
            break;

        case 41: // Write file (arg1 pointer to name of file, arg2 pointer to data, arg3 size of data)
            if (!filesystem_write_file((const char*)arg1, (const uint8_t*)arg2, arg3)) {
                screen_println("Couldn't write file");
            }
            break;
    }
}