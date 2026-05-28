//
// Created by eitan on 5/10/26.
//

#ifndef VMM_H
#define VMM_H

#include "stdint.h"
#include "limine.h"

#define PAGE_SIZE 4096

// All intermediate table entries (PML4, PDPT, PD levels)
#define VMM_FLAGS_TABLE   (1ull << 0) | (1ull << 1) | (1ull << 2)

// Kernel page - readable, writable, no user access, no execute
#define VMM_FLAGS_KERNEL_RW   (1ull << 0) | (1ull << 1) | (1ull << 63)

// Kernel code - readable, executable, no user access, no write
#define VMM_FLAGS_KERNEL_CODE (1ull << 0)

// Kernel all - readable, writeable, executable, no user access
#define VMM_FLAGS_KERNEL_ALL (1ull << 0) | (1ull << 1)

// User page - readable, writable, user accessible, no execute
#define VMM_FLAGS_USER_RW   (1ull << 0) | (1ull << 1) | (1ull << 2) | (1ull << 63)

// User code - readable, writable, user accessible
#define VMM_FLAGS_USER_CODE (1ULL << 0) | (1ULL << 2)

// MMIO - writable, cache disabled, no execute
#define VMM_FLAGS_MMIO   (1ull << 0) | (1ull << 1) | (1ull << 4) | (1ull << 63)

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

bool_t vmm_create_PML4(PML4Table** PML4_ptr);
void vmm_unmap_PML4(PML4Table* PML4);
PML4Table* vmm_init(volatile struct limine_hhdm_request* hhdm_request, volatile struct limine_executable_address_request* kernel_address_request);
void vmm_copy_kernel_PML4(PML4Table* to, PML4Table* from);
void vmm_set_PML4(PML4Table* PML4);
bool_t vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
bool_t vmm_unmap_page(uint64_t virt);
bool_t vmm_is_mapped(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);
void vmm_load_cpu();
bool_t vmm_alloc(uint64_t start_virt, uint64_t end_virt, uint64_t flags);
void vmm_free(uint64_t start_virt, uint64_t end_virt);

#endif //VMM_H
