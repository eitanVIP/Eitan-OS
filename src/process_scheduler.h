//
// Created by eitan on 9/22/25.
//

#pragma once

typedef struct cpu_state {
    unsigned int gs;
    unsigned int fs;
    unsigned int es;
    unsigned int ds;

    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;

    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    // unsigned int useresp;
    // unsigned int ss;
} cpu_state_t;

typedef struct process {
    unsigned int pid;
    cpu_state_t regs;
    unsigned int pending_signals;
    unsigned int stack_start;
    unsigned int eip;
} process_t;

void process_scheduler_init();
void process_scheduler_next_process(unsigned int* current_regs);