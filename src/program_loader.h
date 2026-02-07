//
// Created by eitan on 1/22/26.
//

#ifndef PROGRAM_LOADER_H
#define PROGRAM_LOADER_H

#include "stdint.h"

bool_t program_loader_load_elf32(const uint8_t* file_data, uint32_t* pid);

#endif //PROGRAM_LOADER_H
