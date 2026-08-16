//
// Created by eitan on 1/20/26.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "util/stdint.h"

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

void filesystem_init(void);
bool_t filesystem_read_file(const char* path, uint8_t** data_out, uint32_t* data_size_out, file_entry_t* file_entry_out);
bool_t filesystem_write_file(const char* path, const uint8_t* data, file_type_t type, size_t size);
bool_t filesystem_delete_file(const char* name);
char** filesystem_list_files(const char* path, int* file_count);
char** filesystem_list_dirs(const char* path, int* dir_count);
void filesystem_print_all_entries();

#endif //FILESYSTEM_H
