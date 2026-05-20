//
// Created by eitan on 9/20/25.
//

#pragma once
#include "stdint.h"

// inits heap. should call this per process.
bool_t allocator_heap_init(uint64_t heap_start, uint64_t heap_size, bool_t is_kernel);
// malloc to current process heap. Usually kernel heap but in syscalls- caller's heap.
void* malloc(size_t size);
// malloc specifically to kernel heap.
void* malloc_kernel(size_t size);
// free from current process heap. Usually kernel heap but in syscalls- caller's heap.
void free(void* ptr);
// free specifically from kernel heap.
void free_kernel(void* ptr);
void allocator_print_status();