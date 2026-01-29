//
// Created by eitan on 9/22/25.
//

#include "process_scheduler.h"

#include "gdt.h"
#include "memory.h"
#include "screen.h"

#define STACK_SIZE 16384
#define STACKS_START 0x1000000

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
    unsigned int useresp;
    unsigned int ss;
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

static int highest_pid = 0;
static process_t* current_process;
static stack_t* stack_list;

void process_scheduler_init() {
    current_process = malloc(sizeof(process_t));
    current_process->pid = 0;
    current_process->regs = (cpu_state_t){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    current_process->pending_signals = 0;
    current_process->stack_start = 0;
    current_process->next = current_process;

    stack_list = malloc(sizeof(stack_t));
    stack_list->start = 0;
    stack_list->free = 0;
    stack_list->next = null;
}

void process_scheduler_add_process(void* process_code_start, bool_t is_kernel_level) {
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
        previous_stack->next->start = STACKS_START + (void*)(count * STACK_SIZE);

        new_process->stack_start = previous_stack->next->start;
    }

    unsigned short code_segment;
    unsigned short data_segment;
    if (is_kernel_level) {
        code_segment = gdt_get_index(1, 0, 0);
        data_segment = gdt_get_index(2, 0, 0);
    } else {
        code_segment = gdt_get_index(3, 0, 3);
        data_segment = gdt_get_index(4, 0, 3);
    }
    new_process->regs = (cpu_state_t){ data_segment, data_segment, data_segment, data_segment,
        0, 0, 0, (unsigned int)new_process->stack_start, 0, 0, 0, 0, (unsigned int)process_code_start,
        code_segment, 1 << 1 | 1 << 9 /* bit1 = 1, bit9 = interrupt enabled = 1 */, (unsigned int)new_process->stack_start, data_segment };

    new_process->next = current_process->next;
    current_process->next = new_process;

    asm volatile("sti");
}

void process_scheduler_next_process(unsigned int* current_regs) {
    if ((current_regs[13] & 0x3) != 3) {
        if (current_regs[12] > 0x500000) {
            screen_println("SAVING USER EIP TO KERNEL");
            screen_println_num((double)(uint64_t)current_process->next);
            screen_println_num((double)(uint64_t)current_process);
        }
    }

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

    // ONLY save useresp and ss if we came from Ring 3
    if ((current_regs[13] & 0x3) == 3) {
        current_process->regs.useresp = current_regs[15];
        current_process->regs.ss = current_regs[16];
    } else {
        current_process->regs.useresp = 0;
        current_process->regs.ss = 0;
    }

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

    // ONLY restore useresp and ss if we are GOING to Ring 3
    if ((current_process->regs.cs & 0x3) == 3) {
        current_regs[15] = current_process->regs.useresp;
        current_regs[16] = current_process->regs.ss;
    } else {
        current_regs[15] = 0;
        current_regs[16] = 0;
        if (current_process->regs.eip > 0x500000) {
            screen_println("KERNEL RUNNING USER EIP");
            screen_println_num((double)(uint64_t)current_process);
            screen_println_num((double)(uint64_t)current_process->next);
            screen_println_num((double)(current_process->regs.eip));
            screen_println_num((double)(current_process->next->regs.eip));
        }
    }
}