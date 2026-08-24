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
    size_t size;
    file_type_t type;
    char name[FILE_NAME_LENGTH];
} file_metadata_t;

void filesystem_init(void);
bool_t filesystem_read_file(const char* path, void* data_out);
bool_t filesystem_read_file_metadata(const char* path, file_metadata_t* metadata_out);
bool_t filesystem_file_exists(const char* path);
bool_t filesystem_write_file(const char* path, const uint8_t* data, file_type_t type, size_t size);
bool_t filesystem_delete_file(const char* path);
bool_t filesystem_create_directory(const char* path);
bool_t filesystem_delete_directory(const char* path);
bool_t filesystem_move_file(const char* path, const char* new_path);
file_metadata_t* filesystem_list_dir(const char* path, uint64_t* file_count);
void filesystem_print_state(bool_t include_entries);

#endif //FILESYSTEM_H
