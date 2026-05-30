//
// Created by eitan on 9/20/25.
//

#include "allocator.h"

#include "pmm.h"
#include "../screen.h"
#include "vmm.h"
#include "../util/string.h"
#include "../util/util.h"

#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN-1)) & ~(ALIGN-1))

typedef struct block {
    size_t size;
    struct block* next;
} block_t;

typedef struct {
    block_t** free_list_ptr;
    block_t* free_list;
    bool_t is_kernel;
} allocator_data;

static allocator_data* data;
static block_t** kernel_free_list_ptr;

bool_t allocator_heap_init(uint64_t heap_start, uint64_t heap_size, bool_t is_kernel) {
    // data = (allocator_data*)(heap_start - PAGE_SIZE);
    data = (allocator_data*)(HEAP_START - PAGE_SIZE);
    bool_t success = vmm_alloc((uint64_t)data, (uint64_t)data, VMM_FLAGS_KERNEL_RW);
    if (!success) {
        screen_print("[allocator] failed to map allocator_data page\n");
        return false;
    }
    screen_print("[allocator] mapped allocator_data page\n");

    uint64_t end = heap_start + heap_size - 1;
    success = vmm_alloc(heap_start, end, is_kernel ? VMM_FLAGS_KERNEL_RW : VMM_FLAGS_USER_RW);
    if (!success) {
        vmm_unmap_page(heap_start - PAGE_SIZE);
        screen_print("[allocator] failed to map heap pages\n");
        return false;
    }

    screen_print("[allocator] mapped heap pages\n");

    data->free_list = (block_t*)heap_start;
    data->free_list_ptr = &data->free_list;
    data->is_kernel = is_kernel;
    if (is_kernel)
        kernel_free_list_ptr = data->free_list_ptr;

    data->free_list->size = heap_size;
    data->free_list->next = null;

    screen_print("[allocator] allocator heap init\n");

    return true;
}

void allocator_unmap_heap() {
    for (uint64_t ptr = (uint64_t)data; ptr < (uint64_t)data + PAGE_SIZE + HEAP_SIZE; ptr += PAGE_SIZE) {
        vmm_unmap_page(ptr);
    }
}

void* malloc_internal(block_t** free_list_ptr, size_t size, bool_t is_kernel) {
    if (size == 0) return null;

    block_t* free_list = *free_list_ptr;

    size = ALIGN_UP(size + sizeof(block_t)); // include header
    block_t* prev = null;
    block_t* curr = free_list;

    while (curr) {
        if (curr->size >= size) {
            uint64_t alloc_ptr = (uint64_t)curr + sizeof(block_t);

            // disabled because eager mapping
            // vmm_alloc((uint64_t)curr, (uint64_t)curr + size - 1, is_kernel ? VMM_FLAGS_KERNEL_RW : VMM_FLAGS_USER_RW);

            if (curr->size - size >= sizeof(block_t) + ALIGN) { // if block big enough for another alloc after this one
                // split
                block_t* next = (block_t*)((char*)curr + size);
                next->size = curr->size - size;
                next->next = curr->next;

                if (prev) prev->next = next; else *free_list_ptr = next;

                curr->size = size;
            } else {
                // use entire block
                if (prev) prev->next = curr->next; else *free_list_ptr = curr->next;
            }

            return (void*)alloc_ptr;
        }
        prev = curr;
        curr = curr->next;
    }

    return null; // out of memory
}
void* malloc(size_t size) {
    return malloc_internal(data->free_list_ptr, size, false);
}
void* malloc_kernel(size_t size) {
    return malloc_internal(kernel_free_list_ptr, size, true);
}

static bool_t are_pages_free(uint64_t start, uint64_t size, block_t* free_list) {
    block_t* curr = free_list;
    uint64_t start_page = (start / PAGE_SIZE) * PAGE_SIZE;
    uint64_t end_page = ceil_div(start + size - 1, PAGE_SIZE) * PAGE_SIZE - 1;

    while (curr) {
        uint64_t blk_start = (uint64_t)curr + sizeof(block_t);
        uint64_t blk_end   = (uint64_t)curr + curr->size - 1;
        if (blk_start <= start_page
            && blk_end >= end_page)
            return true;

        curr = curr->next;
    }

    return false;
}

void free_internal(block_t** free_list_ptr, void* ptr) {
    if (!ptr) return;

    block_t* free_list = *free_list_ptr;
    block_t* blk = (block_t*)((char*)ptr - sizeof(block_t));
    uint64_t original_blk_size = blk->size;

    // Insert into sorted free list
    block_t* prev = null;
    block_t* curr = free_list;

    while (curr && curr < blk) {
        prev = curr;
        curr = curr->next;
    }

    blk->next = curr;
    if (prev) prev->next = blk;
    else *free_list_ptr = blk;

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

    // disabled because eager mapping
    // if (are_pages_free((uint64_t)ptr - sizeof(block_t), original_blk_size, free_list))
    //     vmm_free((uint64_t)ptr - sizeof(block_t), (uint64_t)ptr + original_blk_size);
}
void free(void* ptr) {
    free_internal(data->free_list_ptr, ptr);
}
void free_kernel(void* ptr) {
    free_internal(kernel_free_list_ptr, ptr);
}

void allocator_print_status() {
    block_t* curr = data->free_list;
    char buffer[100] = {};

    screen_print("Memory Blocks {\n");
    while (curr != null) {
        char* size_str = num_to_str_no_malloc(curr->size, buffer, sizeof(buffer));
        screen_print("  ");
        screen_print(size_str);
        screen_print("\n");
        curr = curr->next;
    }
    screen_print("}\n");
}