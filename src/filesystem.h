//
// Created by eitan on 1/20/26.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "memory.h"

void filesystem_read_sectors(unsigned int lba, void* target, size_t size);
void filesystem_write_sectors(unsigned int lba, const void* src, size_t size);
void filesystem_init(void);
bool_t filesystem_read_file(const char* name, uint8_t** data_ptr);
bool_t filesystem_write_file(const char* name, const uint8_t* data, size_t size);
bool_t filesystem_delete_file(const char* name);
void filesystem_print_all_entries();

#endif //FILESYSTEM_H
