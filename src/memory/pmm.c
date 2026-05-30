//
// Created by eitan on 5/10/26.
//

#include "pmm.h"

#include "../screen.h"

static uint8_t* pmm_bitmap;
static size_t pmm_bitmap_size;
static size_t total_frames;

uint8_t* pmm_get_bitmap() {
    return pmm_bitmap;
}

size_t pmm_get_bitmap_size() {
    return pmm_bitmap_size;
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

bool_t pmm_reserve_region(void *start, void *end) {
    uint64_t frame_start = (uint64_t)start / FRAME_SIZE;
    uint64_t frame_end   = (uint64_t)end / FRAME_SIZE;

    for (uint64_t i = frame_start; i <= frame_end; i++)
        if (pmm_get_frame(i))
            return false;

    for (uint64_t i = frame_start; i <= frame_end; i++)
        pmm_set_frame(i, true);

    return true;
}

void pmm_free_region(void *start, void *end) {
    uint64_t frame_start = (uint64_t)start / FRAME_SIZE;
    uint64_t frame_end   = (uint64_t)end / FRAME_SIZE;

    for (uint64_t i = frame_start; i <= frame_end; i++)
        pmm_set_frame(i, false);
}

void* pmm_alloc_frame() {
    for (uint64_t i = 0; i < total_frames; i++) {
        if (!pmm_get_frame(i)) {
            pmm_set_frame(i, true);
            return (void*)(i * FRAME_SIZE);
        }
    }

    return null;
}

bool_t pmm_reserve_frame(void* addr) {
    uint64_t frame = (uint64_t)addr / FRAME_SIZE;

    if (pmm_get_frame(frame))
        return false;

    pmm_set_frame(frame, true);

    return true;
}

void pmm_free_frame(void* addr) {
    pmm_set_frame((uint64_t)addr / FRAME_SIZE, false);
}

bool_t pmm_is_reserved(void* addr) {
    return pmm_get_frame((uint64_t)addr / FRAME_SIZE);
}

void pmm_init(volatile struct limine_memmap_request* memmap_request, volatile struct limine_hhdm_request* hhdm_request) {
    struct limine_memmap_response *memmap = memmap_request->response;
    struct limine_hhdm_response *hhdm = hhdm_request->response;

    // 1. Calculate top of RAM to find bitmap size
    uint64_t top_of_ram = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        if (memmap->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t limit = memmap->entries[i]->base + memmap->entries[i]->length;
            if (limit > top_of_ram) top_of_ram = limit;
        }
    }

    screen_print("[pmm] found top of ram: <Can't display numbers yet>\n");

    total_frames = top_of_ram / 4096;
    pmm_bitmap_size = (total_frames + 7) / 8;

    screen_print("[pmm] calculated total physical frames: <Can't display numbers yet>\n");
    screen_print("[pmm] calculated pmm bitmap size: <Can't display numbers yet>\n");

    // Find place for pmm bitmap
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= pmm_bitmap_size) {
            uint64_t bitmap_phys = entry->base;
            // The virtual address (via HHDM):
            pmm_bitmap = (uint8_t *)(bitmap_phys + hhdm->offset);

            // Mark this chunk as no longer usable so the PMM doesn't give it away
            entry->base += pmm_bitmap_size;
            entry->length -= pmm_bitmap_size;
            break;
        }
    }

    screen_print("[pmm] found place for pmm bitmap: <Can't display numbers yet>\n");

    // 3. Now this loop will not crash because pmm_bitmap points
    // into the HHDM which is guaranteed to be mapped and writable.
    for (uint64_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // 4. Free USABLE regions
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            pmm_free_region((void *)entry->base,
                            (void *)(entry->base + entry->length));
        }
    }

    // 5. Re-reserve the bitmap itself
    uint64_t bitmap_phys = (uint64_t)pmm_bitmap - hhdm->offset;
    uint64_t frame_start = bitmap_phys / FRAME_SIZE;
    uint64_t frame_end   = (bitmap_phys + pmm_bitmap_size) / FRAME_SIZE;
    for (uint64_t i = frame_start; i <= frame_end; i++)
        pmm_set_frame(i, true);

    screen_print("[pmm] reserved memory regions\n");
    screen_print("[pmm] pmm init\n");
}