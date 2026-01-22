//
// Created by eitan on 1/22/26.
//

#include "program_loader.h"

#include "eitan_lib.h"
#include "screen.h"
#include "memory.h"
#include "process_scheduler.h"

#define PROCESS_MEMORY_START 0x500000

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
} program_header_entry;

void log(const char* msg) {
    char* final_msg = str_concat("Failed to load elf32 program: ", msg);
    screen_println(final_msg);
    free(final_msg);
}

bool_t check_file(elf32_header header) {
    if (header.magic_number != 0x464C457F) {
        log("Invalid magic number");
        return false;
    }

    if (header.class != 1) {
        log("Invalid class");
        return false;
    }

    if (header.endianness != 1) {
        log("Invalid endianness");
        return false;
    }

    if (header.os_abi != 0) {
        log("Invalid abi");
        return false;
    }

    if (header.type != 2) {
        log("Invalid type");
        return false;
    }

    if (header.machine != 3) {
        log("Invalid instruction set");
        return false;
    }

    if (header.elf_version != 1) {
        log("Invalid elf version");
        return false;
    }

    return true;
}

bool_t program_loader_load_elf32(const uint8_t* file_data) {
    elf32_header header = *(elf32_header*)file_data;
    if (!check_file(header))
        return false;

    for (int i = 0; i < header.program_header_entries; i++) {
        program_header_entry entry = *(program_header_entry*)(file_data + header.program_header_offset + i * header.program_header_entry_size);
        if (entry.segment_type != 1)
            continue;

        if (entry.virtual_address < PROCESS_MEMORY_START) {
            log("Program wants to load out of bounds");
            return false;
        }

        memcpy((void*)entry.virtual_address, file_data + entry.data_offset, entry.file_size);
        memset((uint8_t*)entry.virtual_address + entry.file_size, 0, entry.memory_size - entry.file_size);
    }

    // void (*entry_point)(void) = (void (*)(void))header.entry;
    // entry_point();
    process_scheduler_add_process((void*)header.entry, false);

    return true;
}