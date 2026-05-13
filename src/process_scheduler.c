//
// Created by eitan on 9/22/25.
//

#include "process_scheduler.h"

#include "gdt.h"
#include "allocator.h"
#include "VGA_screen.h"

#define STACK_SIZE 16384
#define STACKS_START 0x1000000

typedef struct {
    uint64_t fs;
    uint64_t gs;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} cpu_state_t;

typedef struct stack {
    void* start;
    unsigned char free;
    struct stack *next;
} stack_t;

typedef struct process {
    uint32_t pid;
    cpu_state_t regs;
    stack_t* stack;
    uint32_t pending_signals;

    struct process* next;
} process_t;

static int32_t highest_pid = 0;
static process_t* current_process;
static stack_t* stack_list;

void process_scheduler_init() {
    stack_list = malloc(sizeof(stack_t));
    stack_list->start = 0;
    stack_list->free = 0;
    stack_list->next = null;

    current_process = malloc(sizeof(process_t));
    current_process->pid = 0;
    current_process->regs = (cpu_state_t){};
    current_process->pending_signals = 0;
    current_process->stack = stack_list;
    current_process->next = current_process;
}

uint32_t process_scheduler_add_process(void* process_code_start, bool_t is_kernel_level) {
    asm volatile("cli");

    process_t* new_process = malloc(sizeof(process_t));

    new_process->pid = ++highest_pid;
    new_process->pending_signals = 0;

    stack_t* current_stack = stack_list;
    stack_t* previous_stack = current_stack;
    bool_t found = false;
    uint32_t count = 0;
    while (current_stack) {
        if (current_stack->free) {
            new_process->stack = current_stack;
            current_stack->free = false;

            found = true;
            break;
        }

        count++;
        previous_stack = current_stack;
        current_stack = current_stack->next;
    }
    if (!found) {
        previous_stack->next = malloc(sizeof(stack_t));
        previous_stack->next->free = false;
        previous_stack->next->start = STACKS_START + (void*)(count * STACK_SIZE);
        previous_stack->next->next = null;

        new_process->stack = previous_stack->next;
    }

    uint16_t code_segment;
    uint16_t data_segment;
    if (is_kernel_level) {
        code_segment = gdt_create_selector(1, 0, 0);
        data_segment = gdt_create_selector(2, 0, 0);
    } else {
        code_segment = gdt_create_selector(3, 0, 3);
        data_segment = gdt_create_selector(4, 0, 3);
    }
    new_process->regs = (cpu_state_t){ data_segment, data_segment,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        (uint32_t)process_code_start, code_segment, 0x202, (uint32_t)new_process->stack->start, data_segment };

    new_process->next = current_process->next;
    current_process->next = new_process;

    asm volatile("sti");
    return new_process->pid;
}

bool_t find_process(uint32_t pid, process_t** process_ptr) {
    *process_ptr = current_process;
    process_t* first_process = *process_ptr;

    do {
        if ((*process_ptr)->pid == pid)
            break;
        *process_ptr = (*process_ptr)->next;
    } while (*process_ptr != first_process);
    if ((*process_ptr)->pid != pid)
        return false;

    return true;
}

bool_t find_previous_process(uint32_t pid, process_t** process_ptr) {
    *process_ptr = current_process;
    process_t* first_process = *process_ptr;

    do {
        if ((*process_ptr)->next->pid == pid)
            break;
        *process_ptr = (*process_ptr)->next;
    } while (*process_ptr != first_process);
    if ((*process_ptr)->next->pid != pid)
        return false;

    return true;
}

bool_t process_scheduler_remove_process(uint32_t pid) {
    asm volatile("cli");

    if (pid == 0)
        return false;

    process_t* process;
    if (!find_previous_process(pid, &process))
        return false;

    process_t* next_next = process->next->next;
    process->next->stack->free = true;
    free(process->next);
    process->next = next_next;
    current_process = process;

    return true;
}

void process_scheduler_exit(uint64_t* current_regs) {
    process_scheduler_remove_process(current_process->pid);

    process_t* kernel_process;
    find_process(0, &kernel_process);
    current_process = kernel_process;

    *(cpu_state_t *)current_regs = current_process->regs;
}

void process_scheduler_send_signals(uint32_t pid, uint32_t signals) {
    process_t* process;
    if (!find_process(pid, &process))
        return;

    process->pending_signals |= signals;
}

bool_t handle_signals(process_t* process) {
    uint32_t sigs = process->pending_signals;
    process->pending_signals = 0;

    if (sigs & SIG_HUP) {

    }
    if (sigs & SIG_INT) {

    }
    if (sigs & SIG_QUIT) {

    }
    if (sigs & SIG_ILL) {

    }
    if (sigs & SIG_TRAP) {

    }
    if (sigs & SIG_ABRT) {
        if (process_scheduler_remove_process(process->pid)) {
            VGA_screen_println("SIGABRT");
            return true;
        }
    }
    if (sigs & SIG_BUS) {

    }
    if (sigs & SIG_FPE) {

    }
    if (sigs & SIG_KILL) {
        if (process_scheduler_remove_process(process->pid)) {
            VGA_screen_println("SIGKILL");
            return true;
        }
    }
    if (sigs & SIG_USR1) {

    }
    if (sigs & SIG_SEGV) {

    }
    if (sigs & SIG_USR2) {

    }
    if (sigs & SIG_PIPE) {

    }
    if (sigs & SIG_ALRM) {

    }
    if (sigs & SIG_TERM) {

    }
    if (sigs & SIG_STKFLT) {

    }
    if (sigs & SIG_CHLD) {

    }
    if (sigs & SIG_CONT) {

    }
    if (sigs & SIG_STOP) {

    }
    if (sigs & SIG_TSTP) {

    }
    if (sigs & SIG_TTIN) {

    }
    if (sigs & SIG_TTOU) {

    }
    if (sigs & SIG_URG) {

    }
    if (sigs & SIG_XCPU) {

    }
    if (sigs & SIG_XFSZ) {

    }
    if (sigs & SIG_VTALRM) {

    }
    if (sigs & SIG_PROF) {

    }
    if (sigs & SIG_WINCH) {

    }
    if (sigs & SIG_IO) {

    }
    if (sigs & SIG_PWR) {

    }
    if (sigs & SIG_SYS) {

    }
    if (sigs & SIG_RT) {

    }

    return false;
}

void process_scheduler_next_process(uint64_t* current_regs) {
    current_process->regs = *(cpu_state_t *)current_regs;

    do {
        current_process = current_process->next;
    } while (handle_signals(current_process));

    *(cpu_state_t *)current_regs = current_process->regs;
}