//
// Created by eitan on 9/24/25.
//

#include "interrupts.h"
#include "../util/io.h"
#include "../VGA_screen.h"
#include "../util/util.h"
#include "../filesystem.h"
#include "process_scheduler.h"
#include "../memory/allocator.h"
#include "../util/panic.h"
#include "program_loader.h"
#include "../screen.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193180

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

void exception_handler_c(uint32_t int_no, uint64_t* regs);
void irq_handler_c(uint32_t int_no, uint64_t* regs);
void syscall_handler_c(uint64_t syscall_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t* current_regs);

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
    uint16_t    isr_low;       // The lower 16 bits of the ISR's address
    uint16_t    kernel_cs;     // GDT Segment Selector
    uint8_t     ist;           // Bits 0..2 hold the Interrupt Stack Table offset
    uint8_t     attributes;    // Type and attributes
    uint16_t    isr_mid;       // The middle 16 bits of the ISR's address
    uint32_t    isr_high;      // The higher 32 bits of the ISR's address
    uint32_t    reserved;      // Set to zero
} __attribute__((packed)) idt_entry_t;

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

__attribute__((aligned(0x10)))
static idt_entry_t idt[256];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];
    uint64_t addr = (uint64_t)isr;

    descriptor->isr_low    = addr & 0xFFFF;
    descriptor->kernel_cs  = 0x08;
    descriptor->ist        = 0;    // 0 means don't use IST (standard behavior)
    descriptor->attributes = flags;
    descriptor->isr_mid    = (addr >> 16) & 0xFFFF;
    descriptor->isr_high   = (addr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

static inline void lidt(void* base, uint16_t size) {
    struct idt_ptr IDTR = { size, (uint64_t) base };
    asm volatile("lidt %0" : : "m"(IDTR));
}

void interrupts_init() {
    // 1. PIC Remap Sequence
    // ---------------------------------------------------------
    // Starts the initialization sequence (ICW1)
    io_outb(0x20, 0x11);
    io_outb(0xA0, 0x11);

    // Set vector offsets (ICW2)
    io_outb(0x21, 0x20); // Master: 0x20-0x27 (32-39)
    io_outb(0xA1, 0x28); // Slave:  0x28-0x2F (40-47)

    // Tell Master PIC about Slave at IRQ2 (ICW3)
    io_outb(0x21, 0x04);
    // Tell Slave PIC its cascade identity (ICW3)
    io_outb(0xA1, 0x02);

    // Set PICs to 8086 mode (ICW4)
    io_outb(0x21, 0x01);
    io_outb(0xA1, 0x01);

    // 2. Explicit Masking (The "Safe Guard")
    // ---------------------------------------------------------
    // We mask everything by default to prevent random hardware noise
    // from jumping to uninitialized handlers.

    uint8_t master_mask = 0xFF; // Start with all bits 1 (masked)
    uint8_t slave_mask  = 0xFF;

    // UNMASK IRQ0 (Timer): Bit 0
    master_mask &= ~(1 << 0);

    // UNMASK IRQ2 (Cascade): Bit 2
    // Must be unmasked for any Slave PIC interrupts (8-15) to work.
    master_mask &= ~(1 << 2);

    io_outb(0x21, master_mask);
    io_outb(0xA1, slave_mask);

    screen_print("[interrupts] remapped PIC\n");

    // 3. PIT Setup (100Hz)
    // ---------------------------------------------------------
    uint16_t divisor = PIT_FREQ / 100;
    io_outb(PIT_COMMAND, 0x36);             // Command port
    io_outb(PIT_CHANNEL0, divisor & 0xFF);   // Low byte
    io_outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); // High byte

    screen_print("[interrupts] configured PIT: 100 Hz\n");

    // IDT
    for (uint16_t i = 0; i < 256; ++i) {
        idt_set_descriptor(i, isr0, 0x8E);
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

    screen_print("[interrupts] configured IDT exceptions\n");

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

    screen_print("[interrupts] configured IDT IRQs\n");

    idt_set_descriptor(0x80, syscall_handler_asm, 0xEE);
    screen_print("[interrupts] configured IDT syscall\n");

    lidt(idt, sizeof(idt) - 1);
    screen_print("[interrupts] loaded IDT\n");

    // Enable interrupts
    asm volatile("sti");
    screen_print("[interrupts] enabled interrupts\n");
    screen_print("[interrupts] interrupts init\n");
}

void send_eoi(uint32_t irq_number) {
    if (irq_number >= 8) {
        // If IRQ came from Slave PIC
        io_outb(PIC2_COMMAND, PIC_EOI); // Send EOI to Slave PIC
    }
    // Always send EOI to Master PIC
    io_outb(PIC1_COMMAND, PIC_EOI); // Send EOI to Master PIC
}

void exception_handler_c(uint32_t int_no, uint64_t* regs) {
    // VGA_screen_print("CPU Exception: ");
    // VGA_screen_println_num(int_no);
    //
    // VGA_screen_print("GS: ");      VGA_screen_println_num(regs[0]);
    // VGA_screen_print("FS: ");      VGA_screen_println_num(regs[1]);
    // VGA_screen_print("ES: ");      VGA_screen_println_num(regs[2]);
    // VGA_screen_print("DS: ");      VGA_screen_println_num(regs[3]);
    //
    // VGA_screen_print("EDI: ");     VGA_screen_println_num(regs[4]);
    // VGA_screen_print("ESI: ");     VGA_screen_println_num(regs[5]);
    // VGA_screen_print("EBP: ");     VGA_screen_println_num(regs[6]);
    // VGA_screen_print("ESP: ");     VGA_screen_println_num(regs[7]);
    // VGA_screen_print("EBX: ");     VGA_screen_println_num(regs[8]);
    // VGA_screen_print("EDX: ");     VGA_screen_println_num(regs[9]);
    // VGA_screen_print("ECX: ");     VGA_screen_println_num(regs[10]);
    // VGA_screen_print("EAX: ");     VGA_screen_println_num(regs[11]);
    //
    // // The Exception Frame
    // VGA_screen_print("Error: ");   VGA_screen_println_num(regs[12]);
    // VGA_screen_print("EIP: ");     VGA_screen_println_num(regs[13]);
    // VGA_screen_print("CS: ");      VGA_screen_println_num(regs[14]);
    // VGA_screen_print("EFLAGS: ");  VGA_screen_println_num(regs[15]);
    //
    // // Only valid if the exception came from User Mode (Ring 3)
    // if ((regs[14] & 0x3) == 3) {
    //     VGA_screen_print("User ESP: "); VGA_screen_println_num(regs[16]);
    //     VGA_screen_print("SS: ");       VGA_screen_println_num(regs[17]);
    // }
    //
    // while (1) {
    //     asm volatile("hlt");
    // }
    panic("exception_handler_c");
}

void irq_handler_c(uint32_t int_no, uint64_t* regs) {
    if (int_no < 32 || int_no > 47) return;

    uint32_t irq = int_no - 32;

    if (irq == 0) {
        process_scheduler_next_process(regs);
    } else if (irq == 1) {
        // Keyboard - disabled in PIC
    } else {
        // Other IRQs - disabled in PIC
    }

    send_eoi(irq);
}

void syscall_handler_c(uint64_t syscall_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t* current_regs) {
    switch (syscall_id) {
        case 0: // Exit
            process_scheduler_exit(current_regs);
            break;

        case 1: // Run program (arg1 pointer to name of file, arg2 pointer to put pid)
            uint8_t* data1;
            uint32_t data_size1;
            if (filesystem_read_file((const char*)arg1, &data1, &data_size1)) {
                program_loader_load_elf32(data1, (uint32_t*)arg2);
            }
            break;

        case 2: // Close process (arg1 pid)
            process_scheduler_send_signals(arg1, SIG_KILL);
            break;



        case 10: // Malloc (arg1 size, arg2 pointer to put pointer to memory)
            *(uint8_t**)arg2 = malloc(arg1);
            break;

        case 11: // Free (arg1 pointer)
            free((void*)arg1);
            break;



        case 20: // Read keyboard char (arg1 pointer to put char)
            // *(uint16_t*)arg1 = io_keyboard_read();
            *(uint16_t*)arg1 = io_keyboard_buffer;
            io_keyboard_buffer = 0;
            break;



        case 30: // Print string (arg1 pointer to string)
            VGA_screen_print((const char*)arg1);
            break;

        case 31: // Clear screen
            VGA_screen_clear_screen();
            break;



        case 40: // Read file (arg1 pointer to name of file, arg2 pointer to put pointer to heap data, arg3 size of data)
            uint8_t* data2;
            uint32_t data_size2;
            if (filesystem_read_file((const char*)arg1, &data2, &data_size2)) {
                *(uint8_t**)arg2 = data2;
                *(uint8_t*)arg3 = data_size2;
            }
            break;

        case 41: // Write file (arg1 pointer to name of file, arg2 pointer to data, arg3 size of data)
            filesystem_write_file((const char*)arg1, (const uint8_t*)arg2, arg3);
            break;

        case 42: // List files (arg1 pointer to name of path, arg2 pointer to put pointer to array of names, arg3 pointer to put array size)
            *(char***)arg2 = filesystem_list_files((char*)arg1, (int*)arg3);
            break;

        case 43: // List dirs (arg1 pointer to name of path, arg2 pointer to put pointer to array of names, arg3 pointer to put array size)
            *(char***)arg2 = filesystem_list_dirs((char*)arg1, (int*)arg3);
            break;

        case 44: // Delete file (arg1 pointer to name of file)
            filesystem_delete_file((const char*)arg1);
            break;

        default:
            break;
    }
}