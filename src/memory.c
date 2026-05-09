//
// Created by eitan on 9/20/25.
//

#include "memory.h"
#include "eitan_lib.h"
#include "screen.h"

extern uint8_t pmm_bitmap[];
extern uint8_t kernel_end[];

#define FRAME_SIZE 4096

#define HEAP_START 0x500000
#define HEAP_END   0x1000000
#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN-1)) & ~(ALIGN-1))

typedef struct block {
    size_t size;        // size >= sizeof(struct block)
    struct block* next; // next free block
} block_t;

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

static size_t pmm_bitmap_size;
static size_t total_frames;
static size_t used_start_frames;

static block_t* free_list = (block_t*)HEAP_START;

double floor(double num) {
    return num >= 0 ? (int)num : ((num - (int)num) != 0 ? (int)num - 1 : num);
}

double ceil(double num) {
    double flo = floor(num);
    double frac = num - flo;
    return frac != 0 ? flo + 1 : num;
}

void pmm_set_frame_used(void* addr, bool_t used) {
    uint64_t ptr = (uint64_t)addr;
    if (used)
        pmm_bitmap[ptr / FRAME_SIZE / 8] |= 1 << (ptr / FRAME_SIZE);
    else
        pmm_bitmap[ptr / FRAME_SIZE / 8] &= 1 << (ptr / FRAME_SIZE) ^ 0xff;
}

bool_t pmm_read_frame_used(void* addr) {
    uint64_t ptr = (uint64_t)addr;

    return pmm_bitmap[ptr / FRAME_SIZE / 8] >> (ptr / FRAME_SIZE) & 1;
}

void pmm_init(multiboot_info_t* multiboot_pointer) {
    total_frames = (0x100000 + multiboot_pointer->mem_upper * 1024) / FRAME_SIZE;
    pmm_bitmap_size = ceil((double)total_frames / 8);

    used_start_frames = ceil((double)(size_t)kernel_end / FRAME_SIZE);

    for (size_t i = 0; i <= used_start_frames * FRAME_SIZE; i += FRAME_SIZE) {
        pmm_set_frame_used((void*)i, true);
    }
    for (size_t i = used_start_frames * FRAME_SIZE; i <= size_t(ceil((double)used_start_frames / 8) * 8); i += FRAME_SIZE) {
        pmm_set_frame_used((void*)i, false);
    }
    for (size_t i = ceil((double)used_start_frames / 8); i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0;
    }
}

uint64_t pmm_alloc_frame() {
    for (size_t i = used_start_frames; i <= total_frames; i++) {
        uint64_t frame_addr = i * FRAME_SIZE;

        if (!pmm_read_frame_used((void*)frame_addr)) {
            pmm_set_frame_used((void*)frame_addr, true);
            return frame_addr;
        }
    }

    return null;
}

void pmm_free_frame(uint64_t addr) {
    pmm_set_frame_used((void*)addr, false);
}

void memory_heap_init(void) {
    free_list->size = HEAP_END - HEAP_START;
    free_list->next = null;
}

void* malloc(size_t size) {
    if (size == 0) return null;
    size = ALIGN_UP(size + sizeof(block_t)); // include header
    block_t* prev = null;
    block_t* curr = free_list;

    while (curr) {
        if (curr->size >= size) {
            if (curr->size - size >= sizeof(block_t) + ALIGN) {
                // split
                block_t* next = (block_t*)((char*)curr + size);
                next->size = curr->size - size;
                next->next = curr->next;

                if (prev) prev->next = next; else free_list = next;

                curr->size = size;
            } else {
                // use entire block
                if (prev) prev->next = curr->next; else free_list = curr->next;
            }
            // return user pointer (after header)
            return (void*)((char*)curr + sizeof(block_t));
        }
        prev = curr;
        curr = curr->next;
    }
    return null; // out of memory
}

void free(void* ptr) {
    if (!ptr) return;

    block_t* blk = (block_t*)((char*)ptr - sizeof(block_t));

    // Insert into sorted free list
    block_t* prev = null;
    block_t* curr = free_list;

    while (curr && curr < blk) {
        prev = curr;
        curr = curr->next;
    }

    blk->next = curr;
    if (prev) prev->next = blk;
    else free_list = blk;

    // Coalesce with next block
    if (blk->next && (char*)blk + blk->size == (char*)blk->next) {
        blk->size += blk->next->size;
        blk->next = blk->next->next;
    }

    // Coalesce with previous block
    if (prev && (char*)prev + prev->size == (char*)blk) {
        prev->size += blk->size;
        prev->next = blk->next;
    }
}

void memory_print_blocks() {
    block_t* curr = free_list;
    screen_print("Memory Blocks:\n");
    while (curr) {
        char* size_str = num_to_str(curr->size);
        screen_print(size_str);
        screen_print("\n");
        free(size_str);
        curr = curr->next;
    }
    screen_print("Memory Blocks End\n");
}