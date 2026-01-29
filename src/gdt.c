//
// Created by eitan on 10/2/25.
//

#include "gdt.h"
#include "stdint.h"
#include "eitan_lib.h"

extern void setGdt(unsigned short limit, unsigned int base);
extern void reloadSegments();
extern void flush_tss();

struct GDT {
    unsigned int base;        // 32-bit base
    unsigned int limit;       // 20-bit limit (stored in 32-bit int)
    unsigned char access_byte;  // Access byte
    unsigned char flags;        // Only 4 bits used, but stored in 8-bit int
};

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t unused[22];
    uint16_t iopb_offset;
} __attribute__((packed));

unsigned char gdt[6 * 8] __attribute__((aligned(8)));
static struct tss_entry kernel_tss;
volatile uint8_t safe_transition_stack[16384] __attribute__((aligned(16)));

void encode_gdt_entry(unsigned char *target, struct GDT source) {
    // Check the limit to make sure that it can be encoded
    if (source.limit > 0xFFFFF)
        return;

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

void gdt_init() {
    encode_gdt_entry(gdt, (struct GDT){ 0, 0, 0, 0 }); // Null desc
    encode_gdt_entry(gdt + 0x0008, (struct GDT){ 0, 0xFFFFF, 0x9A, 0xC }); // Kernel code
    encode_gdt_entry(gdt + 0x0010, (struct GDT){ 0, 0xFFFFF, 0x92, 0xC }); // Kernel data
    encode_gdt_entry(gdt + 0x0018, (struct GDT){ 0, 0xFFFFF, 0xFA, 0xC }); // User code
    encode_gdt_entry(gdt + 0x0020, (struct GDT){ 0, 0xFFFFF, 0xF2, 0xC }); // User data

    // TSS
    uint32_t tss_base = (uint32_t)&kernel_tss;
    uint32_t tss_limit = sizeof(kernel_tss) - 1;

    encode_gdt_entry(gdt + 0x28, (struct GDT){ tss_base, tss_limit, 0x89, 0x0 });

    // Initialize TSS
    memset(&kernel_tss, 0, sizeof(kernel_tss));
    kernel_tss.ss0 = 0x10; // Kernel Data Segment
    kernel_tss.esp0 = (uint32_t)&safe_transition_stack[16383];

    // Load the GDT
    setGdt(6 * 8 - 1, (unsigned int) gdt);
    reloadSegments();

    // Load the TSS selector into the Task Register
    flush_tss();
}

unsigned short gdt_get_index(unsigned char index, unsigned char TI, unsigned char RPL) {
    return (index << 3) | (TI << 2) | RPL;
}