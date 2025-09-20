//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"

#define HEAP_START 0x200000
#define HEAP_END   0x400000

long* allocations = 0x100000 + 0x4000; // OS starts at 1mb, stack ends at 16kb.
long allocations_size = 0;
long heap_top = HEAP_START;

void kernel_main(void) {
    screen_clear_screen();
    const char* msgs[] = { "Eitan OS Started...\n\0", "Hello user!\n\0" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i], strlen(msgs[i]));

    while (1) {
        const char keyboard = io_keyboard_read();
        screen_print(&keyboard, 1);
    }

    while (1) {
        asm volatile("hlt");
    }
}

int reserve_memory(long addr, long size) {
    for (long i = 0; i < allocations_size; i++) {
        long alloc_start = allocations[i * 2];
        long alloc_end = alloc_start + allocations[i * 2 + 1] - 1;
        long addr_end = addr + size - 1;

        if (addr < alloc_end && addr_end > alloc_start)
            return 1;
    }

    allocations[allocations_size * 2] = addr;
    allocations[allocations_size * 2 + 1] = size;
    allocations_size++;
    return 0;
}

int free(long addr) {
    for (long i = 0; i < allocations_size; i++) {
        long alloc_start = allocations[i * 2];
        if (addr == alloc_start) {
            for (long j = i; j < allocations_size - 1; j++) {
                allocations[j * 2] = allocations[j * 2 + 2];
                allocations[j * 2 + 1] = allocations[j * 2 + 3];
            }
            allocations[(allocations_size - 1) * 2] = 0;
            allocations[(allocations_size - 1) * 2 + 1] = 0;

            allocations_size--;
            return 0;
        }
    }

    return 1;
}

long malloc(long size) {
    if (heap_top + size > HEAP_END)
        return 0; // out of memory
    long addr = heap_top;
    heap_top += size;
    if (reserve_memory(heap_top, size) == 1)
        return 0;
    return addr;
}