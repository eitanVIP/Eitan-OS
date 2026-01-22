//
// Created by eitan on 9/22/25.
//

#pragma once

#include "stdint.h"

void process_scheduler_init();
void process_scheduler_add_process(void* process_code_start, bool_t is_kernel_level);
void process_scheduler_next_process(unsigned int* current_regs);
