//
// Created by eitan on 1/20/26.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "util/stdint.h"

#define FILE_NAME_LENGTH 60

typedef enum {
    FILE,
    DIRECTORY,
    ROOT_DIRECTORY,
} file_type_t;

typedef struct {
    uint32_t magic_number;
    file_type_t type;
    uint32_t start_sector;
    uint32_t size;
} __attribute__((packed)) file_entry_t;

typedef struct {
    file_type_t type;
    char name[FILE_NAME_LENGTH];
} dir_listing_t;

void filesystem_init(void);
bool_t filesystem_read_file(const char* path, uint8_t** data_out, size_t* data_size_out);
bool_t filesystem_write_file(const char* path, const uint8_t* data, file_type_t type, size_t size);
bool_t filesystem_delete_file(const char* path);
bool_t filesystem_create_directory(const char* path);
bool_t filesystem_move_file(const char* path, const char* new_path);
dir_listing_t* filesystem_list_dir(const char* path, uint64_t* file_count);
void filesystem_print_state(bool_t include_entries);

#endif //FILESYSTEM_H
