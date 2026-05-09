//
// Created by eitan on 5/10/26.
//

#include "pmm.h"

#define FRAME_SIZE 4096
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

extern uint8_t kernel_end[];

static uint8_t* pmm_bitmap;
static size_t pmm_bitmap_size;
static size_t total_frames;

uint64_t ceil_div(uint64_t numerator, uint64_t denominator) {
    return (numerator + denominator - 1) / denominator;
}

void pmm_set_frame(uint64_t frame, bool_t used) {
    if (used)
        pmm_bitmap[frame / 8] |= (1 << (frame % 8));
    else
        pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

bool_t pmm_get_frame(uint64_t frame) {
    return (pmm_bitmap[frame / 8] >> (frame % 8)) & 1;
}

void pmm_reserve_region(void *start, void *end) {
    uint64_t frame_start = (uint64_t)start / FRAME_SIZE;
    uint64_t frame_end   = ((uint64_t)end + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint64_t i = frame_start; i < frame_end; i++)
        pmm_set_frame(i, true);
}

void pmm_free_region(void *start, void *end) {
    uint64_t frame_start = (uint64_t)start / FRAME_SIZE;
    uint64_t frame_end   = ((uint64_t)end + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint64_t i = frame_start; i < frame_end; i++)
        pmm_set_frame(i, false);
}

uint64_t pmm_alloc_frame() {
    for (uint64_t i = 0; i < total_frames; i++) {
        if (!pmm_get_frame(i)) {
            pmm_set_frame(i, true);
            return i;
        }
    }

    return null;
}

void pmm_free_frame(uint64_t frame) {
    pmm_set_frame(frame, false);
}

void pmm_init(multiboot_info_t *mb) {
    // Place bitmap right after the kernel
    pmm_bitmap = (uint8_t *)&kernel_end;

    // Find top of RAM by walking the mmap
    uint64_t top_of_ram = 0;
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)(uint64_t)mb->mmap_addr;
    multiboot_mmap_entry_t *mmap_end = (multiboot_mmap_entry_t *)(uint64_t)(mb->mmap_addr + mb->mmap_length);
    while (entry < mmap_end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t region_top = entry->base_addr + entry->length;
            if (region_top > top_of_ram)
                top_of_ram = region_top;
        }
        entry = (multiboot_mmap_entry_t *)((uint8_t *)entry + entry->size + sizeof(uint32_t));
    }

    total_frames = top_of_ram / FRAME_SIZE;
    pmm_bitmap_size = ceil_div(total_frames, 8);

    // Assume everything is used
    for (uint64_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // Free only what the mmap says is available
    entry = (multiboot_mmap_entry_t *)(uint64_t)mb->mmap_addr;
    while (entry < mmap_end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
            pmm_free_region((void *)entry->base_addr,
                            (void *)(entry->base_addr + entry->length));
        entry = (multiboot_mmap_entry_t *)((uint8_t *)entry + entry->size + sizeof(uint32_t));
    }

    // Re-reserve what we're actually using inside available RAM
    pmm_reserve_region((void *)0,           (void *)0x100000);                    // low memory
    pmm_reserve_region((void *)&kernel_end, pmm_bitmap + pmm_bitmap_size);        // kernel + bitmap
    pmm_reserve_region(mb,                  (uint8_t *)mb + sizeof(multiboot_info_t)); // multiboot struct
}