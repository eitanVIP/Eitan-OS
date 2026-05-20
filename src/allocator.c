//
// Created by eitan on 9/20/25.
//

#include "allocator.h"

#include "pmm.h"
#include "vmm.h"

#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN-1)) & ~(ALIGN-1))

typedef struct block {
    size_t size;
    struct block* next;
} block_t;

typedef struct {
    block_t* free_list;
    bool_t is_kernel;
} allocator_data;

static allocator_data* data;
static block_t* kernel_free_list;

bool_t allocator_heap_init(uint64_t heap_start, uint64_t heap_size, bool_t is_kernel) {
    data = (allocator_data*)(heap_start - PAGE_SIZE);
    bool_t success = vmm_alloc((uint64_t)data, (uint64_t)data, VMM_FLAGS_KERNEL_RW);
    if (!success) return false;

    uint64_t end = heap_start + heap_size - 1;
    success = vmm_alloc(heap_start, end, is_kernel ? VMM_FLAGS_KERNEL_RW : VMM_FLAGS_USER_RW);
    if (!success) {
        vmm_unmap_page(heap_start - PAGE_SIZE);
        return false;
    }

    data->free_list = (block_t*)heap_start;
    data->is_kernel = is_kernel;
    if (is_kernel)
        kernel_free_list = (block_t*)heap_start;

    data->free_list->size = heap_size;
    data->free_list->next = null;

    return true;
}

void* malloc_internal(block_t* free_list, size_t size, bool_t is_kernel) {
    if (size == 0) return null;

    size = ALIGN_UP(size + sizeof(block_t)); // include header
    block_t* prev = null;
    block_t* curr = free_list;

    while (curr) {
        if (curr->size >= size) {
            uint64_t alloc_ptr = (uint64_t)curr + sizeof(block_t);
            if (!vmm_is_mapped(alloc_ptr)) {
                if (!vmm_alloc(alloc_ptr, alloc_ptr + size - 1, is_kernel ? VMM_FLAGS_KERNEL_RW : VMM_FLAGS_USER_RW)) {
                    return null;
                }
            } // TODO: maybe check pmm too

            if (curr->size - size >= sizeof(block_t) + ALIGN) { // if block big enough for another alloc after this one
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

            return (void*)alloc_ptr;
        }
        prev = curr;
        curr = curr->next;
    }

    return null; // out of memory
}
void* malloc(size_t size) {
    return malloc_internal(data->free_list, size, false);
}
void* malloc_kernel(size_t size) {
    return malloc_internal(kernel_free_list, size, true);
}

void free_internal(block_t* free_list, void* ptr) {
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
void free(void* ptr) {
    free_internal(data->free_list, ptr);
}
void free_kernel(void* ptr) {
    free_internal(kernel_free_list, ptr);
}

void allocator_print_status() {
    // block_t* curr = free_list;
    // VGA_screen_print("Memory Blocks:\n");
    // while (curr) {
    //     char* size_str = num_to_str(curr->size);
    //     VGA_screen_print(size_str);
    //     VGA_screen_print("\n");
    //     free(size_str);
    //     curr = curr->next;
    // }
    // VGA_screen_print("Memory Blocks End\n");
}