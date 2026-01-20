//
// Created by eitan on 9/22/25.
//

#include "process_scheduler.h"

#include "gdt.h"
#include "memory.h"

// #define MAX_PROCESSES 100
#define STACK_SIZE 16384

typedef struct {
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
    void* stack_start;
    unsigned int pending_signals;

    struct process* next;
} process_t;

typedef struct stack {
    void* start;
    unsigned char free;
    struct stack *next;
} stack_t;

// static process_t processes[MAX_PROCESSES];
// static int process_count = 1;
static int highest_pid = 0;
static process_t* current_process;
static stack_t* stack_list;

void process_scheduler_init() {
    // processes[0] = (process_t) {
    //     0,
    //     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    //     0
    // };
    // current_process = &processes[0];
    current_process = malloc(sizeof(process_t));
    current_process->pid = 0;
    current_process->regs = (cpu_state_t){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    current_process->pending_signals = 0;
    current_process->stack_start = (void*)0x10401C;
    current_process->next = current_process;

    stack_list = malloc(sizeof(stack_t));
    stack_list->start = (void*)0x10401C;
}

void process_scheduler_add_process(void* process_code_start) {
    asm volatile("cli");

    process_t* new_process = malloc(sizeof(process_t));

    new_process->pid = highest_pid++;
    new_process->pending_signals = 0;

    stack_t* current_stack = stack_list;
    stack_t* previous_stack = current_stack;
    unsigned char found = 0;
    unsigned int count = 0;
    while (current_stack) {
        if (current_stack->free) {
            new_process->stack_start = current_stack->start;
            current_stack->free = 0;

            found = 1;
            break;
        }

        count++;
        previous_stack = current_stack;
        current_stack = current_stack->next;
    }
    if (!found) {
        previous_stack->next = malloc(sizeof(stack_t));
        previous_stack->next->free = 0;
        previous_stack->next->start = (void*)((count + 1) * STACK_SIZE);

        new_process->stack_start = previous_stack->next->start;
    }

    unsigned short kernel_code_segment = gdt_get_index(1, 0, 0);
    unsigned short kernel_data_segment = gdt_get_index(2, 0, 0);
    new_process->regs = (cpu_state_t){ kernel_data_segment, kernel_data_segment, kernel_data_segment, kernel_data_segment,
        0, 0, 0, (unsigned int)new_process->stack_start, 0, 0, 0, 0, (unsigned int)process_code_start,
        kernel_code_segment, 1 << 1 | 1 << 9 /* bit1 = 1, bit9 = interrupt enabled = 1 */ };

    new_process->next = current_process->next;
    current_process->next = new_process;

    asm volatile("sti");
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

    // int nextPID = (current_process->pid + 1) % process_count;
    // current_process = &processes[nextPID];
    current_process = current_process->next;

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