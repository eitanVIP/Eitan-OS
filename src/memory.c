//
// Created by eitan on 9/20/25.
//

#include "memory.h"
#include "eitan_lib.h"
#include "screen.h"

#define HEAP_START 0x200000
#define HEAP_END   0x400000
#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN-1)) & ~(ALIGN-1))

typedef struct block {
    size_t size;        // size >= sizeof(struct block)
    struct block* next; // next free block
} block_t;

static block_t* free_list = (block_t*)HEAP_START;

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