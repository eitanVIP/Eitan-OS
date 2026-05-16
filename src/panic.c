//
// Created by eitan on 5/16/26.
//

#include "panic.h"

#include "screen.h"

void panic(const char *msg) {
    if (screen_is_init()) {
        screen_print("[kernel] PANIC: ");
        screen_print(msg);
    }

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}
