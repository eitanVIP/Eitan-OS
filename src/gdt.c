//
// Created by eitan on 10/2/25.
//

#include "gdt.h"

#include "screen.h"
#include "util/stdint.h"

extern void setGdt(uint16_t limit, uint64_t base);
extern void reloadSegments();
extern void flush_tss();

typedef struct {
    uint32_t base;
    uint32_t limit;         // 20-bit limit (stored in 32-bit int)
    uint8_t access_byte;  // Access byte
    uint8_t flags;        // Only 4 bits used, but stored in 8-bit int
} GDT_entry;

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];        // interrupt stack table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

uint8_t gdt[7 * 8] __attribute__((aligned(8)));
static struct tss_entry kernel_tss;
volatile uint8_t safe_transition_stack[16384] __attribute__((aligned(16)));

void encode_gdt_entry(uint8_t *target, GDT_entry source) {
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
    encode_gdt_entry(gdt, (GDT_entry){ 0, 0, 0, 0 }); // Null desc
    encode_gdt_entry(gdt + 0x0008, (GDT_entry){ 0, 0xFFFFF, 0x9A, 0xA }); // Kernel code
    encode_gdt_entry(gdt + 0x0010, (GDT_entry){ 0, 0xFFFFF, 0x92, 0xC }); // Kernel data
    encode_gdt_entry(gdt + 0x0018, (GDT_entry){ 0, 0xFFFFF, 0xFA, 0xA }); // User code
    encode_gdt_entry(gdt + 0x0020, (GDT_entry){ 0, 0xFFFFF, 0xF2, 0xC }); // User data

    screen_print("[gdt] encoded gdt entries\n");

    // TSS
    uint64_t tss_base = (uint64_t)&kernel_tss;
    uint32_t tss_limit = sizeof(kernel_tss) - 1;

    encode_gdt_entry(gdt + 0x0028, (GDT_entry){ (uint32_t)tss_base, tss_limit, 0x89, 0x0 });
    // Upper 32 bits of TSS base go in the next 8 bytes
    gdt[0x30] = (tss_base >> 32) & 0xFF;
    gdt[0x31] = (tss_base >> 40) & 0xFF;
    gdt[0x32] = (tss_base >> 48) & 0xFF;
    gdt[0x33] = (tss_base >> 56) & 0xFF;
    // gdt[0x34..0x37] = 0 (reserved, already zero from BSS)

    // Initialize TSS
    for (int i = 0; i < sizeof(kernel_tss); ++i) {
        ((uint8_t*)&kernel_tss)[i] = 0;
    }
    kernel_tss.rsp0 = (uint64_t)&safe_transition_stack[16384];

    screen_print("[gdt] encoded tss entry\n");

    // Load the GDT
    setGdt(7 * 8 - 1, (uint64_t) gdt);
    screen_print("[gdt] loaded gdt\n");
    reloadSegments();
    screen_print("[gdt] reloaded segments\n");

    // Load the TSS selector into the Task Register
    flush_tss();
    screen_print("[gdt] flushed tss\n");

    screen_print("[gdt] gdt init\n");
}

// index - index in gdt table
// TI - 0 use gdt, 1 use ldt
// RPL - 0 kernel mode, 3 user mode
uint16_t gdt_create_selector(unsigned char index, unsigned char TI, unsigned char RPL) {
    return (index << 3) | (TI << 2) | RPL;
}