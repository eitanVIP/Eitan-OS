//
// Created by eitan on 9/20/25.
//

#pragma once
#include "stdint.h"

typedef struct {
    uint64_t present         : 1;   // Bit 0
    uint64_t writeable       : 1;   // Bit 1
    uint64_t user_accessible : 1;   // Bit 2
    uint64_t write_through   : 1;   // Bit 3
    uint64_t cache_disabled  : 1;   // Bit 4 (Set this for MMIO!)
    uint64_t accessed        : 1;   // Bit 5
    uint64_t dirty           : 1;   // Bit 6
    uint64_t huge_page       : 1;   // Bit 7
    uint64_t global          : 1;   // Bit 8
    uint64_t ignored_1       : 3;   // Bits 9-11
    uint64_t physical_addr   : 40;  // Bits 12-51 (The actual address!)
    uint64_t ignored_2       : 11;  // Bits 52-62
    uint64_t no_execute      : 1;   // Bit 63
} __attribute__((packed)) page_entry_t;

// Level 4: The Root
typedef struct {
    page_entry_t entries[512];
} PML4Table;

// Level 3
typedef struct {
    page_entry_t entries[512];
} PDPTable;

// Level 2
typedef struct {
    page_entry_t entries[512];
} PageDirectory;

// Level 1: The Leaf (points to actual RAM)
typedef struct {
    page_entry_t entries[512];
} PageTable;

void memory_heap_init(void);
void* malloc(size_t size);
void free(void* ptr);
void memory_print_blocks();