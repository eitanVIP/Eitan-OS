//
// Created by eitan on 9/22/25.
//

#pragma once

typedef struct cpu_state {
    // segments pushed by the stub
    unsigned int gs;
    unsigned int fs;
    unsigned int es;
    unsigned int ds;

    // general purpose registers pushed by pushad (saved in this order)
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;   // original esp before pushad
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;

    // interrupt number and error code could be pushed here if you choose
    unsigned int int_no;
    unsigned int err_code;

    // values pushed by the CPU when the interrupt occurred
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int useresp; // present only if privilege level change occurred
    unsigned int ss;      // present only if privilege level change occurred
} cpu_state_t;

typedef struct process {
    unsigned int pid;
    cpu_state_t regs;
    unsigned int pending_signals;
} process_t;

void process_scheduler_init();
void send_eoi(unsigned int irq_number);