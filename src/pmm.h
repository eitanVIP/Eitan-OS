//
// Created by eitan on 5/10/26.
//

#ifndef PMM_H
#define PMM_H

#include "stdint.h"

typedef struct {
    uint32_t flags;             // which fields below are valid

    // valid if flags bit 0 set
    uint32_t mem_lower;         // KB of low memory (usually 640)
    uint32_t mem_upper;         // KB of upper memory (your RAM - 1MB)

    // valid if flags bit 1 set
    uint32_t boot_device;       // BIOS drive number

    // valid if flags bit 2 set
    uint32_t cmdline;           // physical address of kernel command line string

    // valid if flags bit 3 set
    uint32_t mods_count;        // number of boot modules loaded
    uint32_t mods_addr;         // physical address of first module structure

    // valid if flags bit 4 OR 5 set (mutually exclusive)
    uint32_t syms[4];           // symbol table info (a.out or ELF)

    // valid if flags bit 6 set  ← THIS IS WHAT YOU NEED
    uint32_t mmap_length;       // size of the memory map in bytes
    uint32_t mmap_addr;         // physical address of the memory map

    // valid if flags bit 7 set
    uint32_t drives_length;
    uint32_t drives_addr;

    // valid if flags bit 8 set
    uint32_t config_table;

    // valid if flags bit 9 set
    uint32_t boot_loader_name;  // physical address of bootloader name string

    // valid if flags bit 10 set
    uint32_t apm_table;

    // valid if flags bit 11 set
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    // valid if flags bit 12 set
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_color_info[6];
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;       // size of this entry, NOT counting this field
    uint64_t base_addr;  // start of region
    uint64_t length;     // size of region in bytes
    uint32_t type;       // see below
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif //PMM_H
