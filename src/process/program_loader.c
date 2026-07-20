//
// Created by eitan on 1/22/26.
//

#include "program_loader.h"

#include "../util/util.h"
#include "../VGA_screen.h"
#include "../memory/allocator.h"
#include "process_scheduler.h"
#include "../screen.h"
#include "../util/string.h"

#define PROCESS_MEMORY_START 0x2000000

typedef struct {
    uint32_t magic_number;
    uint8_t class;
    uint8_t endianness;
    uint8_t header_version;
    uint8_t os_abi;
    uint8_t padding[7];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
    uint32_t entry;
    uint32_t program_header_offset;
    uint32_t section_header_offset;
    uint32_t flags;
    uint16_t size;
    uint16_t program_header_entry_size;
    uint16_t program_header_entries;
    uint16_t section_header_entry_size;
    uint16_t section_header_entries;
    uint16_t section_header_str_idx;
} elf32_header;

typedef struct {
    uint32_t segment_type;
    uint32_t data_offset;
    uint32_t virtual_address;
    uint32_t physical_address;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t flags;
    uint32_t alignment;
} efl32_program_header_entry;

typedef struct {
    uint32_t magic_number;
    uint8_t class;
    uint8_t endianness;
    uint8_t header_version;
    uint8_t os_abi;
    uint8_t padding[7];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t size;
    uint16_t program_header_entry_size;
    uint16_t program_header_entries;
    uint16_t section_header_entry_size;
    uint16_t section_header_entries;
    uint16_t section_header_str_idx;
} elf64_header;

typedef struct {
    uint32_t segment_type;
    uint32_t flags;
    uint64_t data_offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} elf64_program_header_entry;

void log_error(const char* msg) {
    char* msgs[] = { "[program_loader] failed to load elf program: ", msg, "\n" };
    char* final_msg = str_concats(msgs, sizeof(msgs) / sizeof(msgs[0]));
    screen_print(final_msg);
    free(final_msg);
}

bool_t check_file_elf32(elf32_header header) {
    if (header.magic_number != 0x464C457F) {
        log_error("Invalid magic number");
        return false;
    }

    if (header.class != 1) {
        log_error("Invalid class");
        return false;
    }

    if (header.endianness != 1) {
        log_error("Invalid endianness");
        return false;
    }

    if (header.os_abi != 0) {
        log_error("Invalid abi");
        return false;
    }

    if (header.type != 2) {
        log_error("Invalid type");
        return false;
    }

    if (header.machine != 3) {
        log_error("Invalid instruction set");
        return false;
    }

    if (header.elf_version != 1) {
        log_error("Invalid elf version");
        return false;
    }

    return true;
}

bool_t program_loader_load_elf32(const uint8_t* file_data, uint32_t* pid) {
    // elf32_header header = *(elf32_header*)file_data;
    // if (!check_file_elf32(header))
    //     return false;
    //
    // for (int i = 0; i < header.program_header_entries; i++) {
    //     efl32_program_header_entry entry = *(efl32_program_header_entry*)(file_data + header.program_header_offset + i * header.program_header_entry_size);
    //     if (entry.segment_type != 1)
    //         continue;
    //
    //     if (entry.virtual_address < PROCESS_MEMORY_START) {
    //         log_error("Program wants to load out of bounds");
    //         return false;
    //     }
    //
    //     memcpy((void*)entry.virtual_address, file_data + entry.data_offset, entry.file_size);
    //     memset((uint8_t*)entry.virtual_address + entry.file_size, 0, entry.memory_size - entry.file_size);
    // }
    //
    // *pid = process_scheduler_add_process((void*)header.entry, false);
    //
    // return true;

    log_error("32 bit program loading is deprecated");
    return false;
}

bool_t check_file_elf64(elf64_header header) {
    if (header.magic_number != 0x464C457F) {
        log_error("Invalid magic number");
        return false;
    }

    if (header.class != 2) {
        log_error("Invalid class (Not 64-bit)");
        return false;
    }

    if (header.endianness != 1) {
        log_error("Invalid endianness");
        return false;
    }

    if (header.os_abi != 0) {
        log_error("Invalid abi");
        return false;
    }

    if (header.type != 2) {
        log_error("Invalid type");
        return false;
    }

    if (header.machine != 62) {
        log_error("Invalid instruction set (Not x86-64)");
        return false;
    }

    if (header.elf_version != 1) {
        log_error("Invalid elf version");
        return false;
    }

    return true;
}

bool_t program_loader_load_elf64(const uint8_t* file_data, uint32_t* pid) {
    screen_print("[program_loader] checking elf64 header\n");

    elf64_header header = *(elf64_header*)file_data;
    if (!check_file_elf64(header))
        return false;

    screen_print("[program_loader] elf64 header OK\n");

    asm volatile("cli");

    screen_print("[program_loader] creating PML4 for process\n");

    PML4Table* new_PML4;
    if (!vmm_create_PML4(&new_PML4)) {
        log_error("Failed to create PML4");
        return false;
    }
    vmm_copy_kernel_PML4(new_PML4, process_scheduler_get_kernel_PML4());
    vmm_set_PML4(new_PML4);
    vmm_load_cpu();

    screen_print("[program_loader] created PML4\n");

    for (int i = 0; i < header.program_header_entries; i++) {
        elf64_program_header_entry entry = *(elf64_program_header_entry*)(file_data + header.program_header_offset + i * header.program_header_entry_size);

        screen_print("[program_loader] reading header entry\n");

        // 1 = PT_LOAD
        if (entry.segment_type != 1) {
            screen_print("[program_loader] entry type is not PT_LOAD. skipping...\n");
            continue;
        }

        if (entry.virtual_address < PROCESS_MEMORY_START) {
            log_error("Program wants to load out of bounds");

            vmm_set_PML4(process_scheduler_get_kernel_PML4());
            vmm_load_cpu();
            vmm_unmap_PML4(new_PML4);

            return false;
        }

        vmm_alloc(entry.virtual_address, entry.virtual_address + entry.memory_size - 1, VMM_FLAGS_KERNEL_ALL);

        // Copies segment data into the virtual address space
        memcpy((void*)entry.virtual_address, file_data + entry.data_offset, entry.file_size);

        // Zero out the BSS section (memory size - file size gap)
        memset((uint8_t*)entry.virtual_address + entry.file_size, 0, entry.memory_size - entry.file_size);

        bool_t is_code = (entry.flags & 0x1) != 0;
        bool_t is_writable_data = (entry.flags & 0x2) != 0;
        uint64_t alloc_flags = 0;
        if (is_code) alloc_flags = VMM_FLAGS_USER_CODE;
        else alloc_flags = VMM_FLAGS_USER_RW;
        vmm_edit_pages(entry.virtual_address, entry.virtual_address + entry.memory_size - 1, alloc_flags);

        screen_print("[program_loader] copied entry to memory\n");
    }

    vmm_alloc(STACK_START - STACK_SIZE + 1, STACK_START, VMM_FLAGS_USER_RW);
    screen_print("[program_loader] mapped stack for process\n");

    screen_print("[program_loader] initiating heap for process:\n");
    allocator_heap_init(HEAP_START, HEAP_SIZE, false);

    vmm_set_PML4(process_scheduler_get_kernel_PML4());
    vmm_load_cpu();

    // Passes the 64-bit entry point address to the process scheduler
    *pid = process_scheduler_add_process((void*)header.entry, new_PML4, false);

    screen_print("[program_loader] added process to scheduler\n");

    return true;
}
