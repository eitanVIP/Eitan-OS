//
// Created by eitan on 9/22/25.
//

#pragma once

#include "stdint.h"

#define SIG_HUP      (1u << 0)
#define SIG_INT      (1u << 1)
#define SIG_QUIT     (1u << 2)
#define SIG_ILL      (1u << 3)
#define SIG_TRAP     (1u << 4)
#define SIG_ABRT     (1u << 5)
#define SIG_BUS      (1u << 6)
#define SIG_FPE      (1u << 7)
#define SIG_KILL     (1u << 8)
#define SIG_USR1     (1u << 9)
#define SIG_SEGV     (1u << 10)
#define SIG_USR2     (1u << 11)
#define SIG_PIPE     (1u << 12)
#define SIG_ALRM     (1u << 13)
#define SIG_TERM     (1u << 14)
#define SIG_STKFLT   (1u << 15)
#define SIG_CHLD     (1u << 16)
#define SIG_CONT     (1u << 17)
#define SIG_STOP     (1u << 18)
#define SIG_TSTP     (1u << 19)
#define SIG_TTIN     (1u << 20)
#define SIG_TTOU     (1u << 21)
#define SIG_URG      (1u << 22)
#define SIG_XCPU     (1u << 23)
#define SIG_XFSZ     (1u << 24)
#define SIG_VTALRM   (1u << 25)
#define SIG_PROF     (1u << 26)
#define SIG_WINCH    (1u << 27)
#define SIG_IO       (1u << 28)
#define SIG_PWR      (1u << 29)
#define SIG_SYS      (1u << 30)
#define SIG_RT       (1u << 31)

void process_scheduler_init();
uint32_t process_scheduler_add_process(void* process_code_start, bool_t is_kernel_level);
void process_scheduler_next_process(unsigned int* current_regs);
bool_t process_scheduler_remove_process(uint32_t pid);
void process_scheduler_send_signals(uint32_t pid, uint32_t signals);
void process_scheduler_exit();