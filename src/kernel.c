//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "interrupts.h"
#include "memory.h"
#include "process_scheduler.h"

void kernel_main(void) {
    memory_heap_init();
    interrupts_init();

    // screen_clear_screen();
    // const char* msgs[] = { "Eitan OS Started...\n", "Hello user!\n" };
    // for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
    //     screen_print(msgs[i], strlen(msgs[i]));
    //
    //
    // while (1) {
    //     const char keyboard = io_keyboard_read();
    //     screen_print(&keyboard, 1);
    // }

    while (1) {
        asm volatile("hlt");
    }
}
