//
// Created by eitan on 9/22/25.
//

#include "process_scheduler.h"
#include "memory.h"

#define MAX_PROCESSES 100

static process_t processes[MAX_PROCESSES];
static int process_count = 1;
static process_t* current_process;

void process_scheduler_init() {
    processes[0] = (process_t) {
        0,
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        0
    };
    current_process = &processes[0];
}

void process_scheduler_next_process(unsigned int* current_regs) {
    current_process->regs.gs = current_regs[0];
    current_process->regs.fs = current_regs[1];
    current_process->regs.es = current_regs[2];
    current_process->regs.ds = current_regs[3];
    current_process->regs.edi = current_regs[4];
    current_process->regs.esi = current_regs[5];
    current_process->regs.ebp = current_regs[6];
    current_process->regs.esp = current_regs[7];
    current_process->regs.ebx = current_regs[8];
    current_process->regs.edx = current_regs[9];
    current_process->regs.ecx = current_regs[10];
    current_process->regs.eax = current_regs[11];
    current_process->regs.eip = current_regs[12];
    current_process->regs.cs = current_regs[13];
    current_process->regs.eflags = current_regs[14];

    int nextPID = (current_process->pid + 1) % process_count;
    current_process = &processes[nextPID];

    current_regs[0] = current_process->regs.gs;
    current_regs[1] = current_process->regs.fs;
    current_regs[2] = current_process->regs.es;
    current_regs[3] = current_process->regs.ds;
    current_regs[4] = current_process->regs.edi;
    current_regs[5] = current_process->regs.esi;
    current_regs[6] = current_process->regs.ebp;
    current_regs[7] = current_process->regs.esp;
    current_regs[8] = current_process->regs.ebx;
    current_regs[9] = current_process->regs.edx;
    current_regs[10] = current_process->regs.ecx;
    current_regs[11] = current_process->regs.eax;
    current_regs[12] = current_process->regs.eip;
    current_regs[13] = current_process->regs.cs;
    current_regs[14] = current_process->regs.eflags;
}