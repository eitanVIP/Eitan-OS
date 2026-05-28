//
// Created by eitan on 9/20/25.
//

#pragma once
#include "stdint.h"

#define HEAP_START_KERNEL 0xffffc90000000000
#define HEAP_START 0x0000555555554000
#define HEAP_SIZE 0x100000

// inits heap. should call this per process.
bool_t allocator_heap_init(uint64_t heap_start, uint64_t heap_size, bool_t is_kernel);

// unmaps the current process's heap.
void allocator_unmap_heap();

// malloc to current process heap. Usually kernel heap but in syscalls- caller's heap.
void* malloc(size_t size);

// malloc specifically to kernel heap.
void* malloc_kernel(size_t size);

// free from current process heap. Usually kernel heap but in syscalls- caller's heap.
void free(void* ptr);

// free specifically from kernel heap.
void free_kernel(void* ptr);

void allocator_print_status();