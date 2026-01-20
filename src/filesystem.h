//
// Created by eitan on 1/20/26.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "memory.h"

void filesystem_read_sectors(unsigned int lba, void* target, size_t size);
void filesystem_write_sectors(unsigned int lba, void* src, size_t size);
void filesystem_init(void);
int filesystem_read_file(char* name, unsigned char** data_ptr);

#endif //FILESYSTEM_H
