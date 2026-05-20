//
// Created by eitan on 5/10/26.
//

#ifndef PMM_H
#define PMM_H

#include "stdint.h"
#include "limine.h"

#define FRAME_SIZE 4096

uint8_t* pmm_get_bitmap();
size_t pmm_get_bitmap_size();
bool_t pmm_reserve_region(void *start, void *end);
void pmm_free_region(void *start, void *end);
void* pmm_alloc_frame();
bool_t pmm_reserve_frame(void* addr);
void pmm_free_frame(void* addr);
bool_t pmm_is_reserved(void* addr);
void pmm_init(volatile struct limine_memmap_request* memmap_request, volatile struct limine_hhdm_request* hhdm_request);

#endif //PMM_H
