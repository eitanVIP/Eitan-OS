//
// Created by eitan on 9/22/25.
//

#pragma once

void process_scheduler_init();
void process_scheduler_add_process(void* process_code_start);
void process_scheduler_next_process(unsigned int* current_regs);
