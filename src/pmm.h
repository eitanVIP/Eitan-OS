//
// Created by eitan on 5/10/26.
//

#ifndef PMM_H
#define PMM_H

#include "stdint.h"
#include "limine.h"

bool_t pmm_reserve_region(void *start, void *end);
void pmm_free_region(void *start, void *end);
void* pmm_alloc_frame();
bool_t pmm_alloc_frame(void* addr);
void pmm_free_frame(void* frame);
bool_t pmm_is_reserved(void* addr);
void pmm_init(struct limine_memmap_request* memmap_request, struct limine_hhdm_request* hhdm_request);

#endif //PMM_H
