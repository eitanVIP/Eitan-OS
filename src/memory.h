//
// Created by eitan on 9/20/25.
//

#pragma once
#include "stdint.h"

void memory_heap_init(void);
void* malloc(size_t size);
void free(void* ptr);
void memory_print_blocks();