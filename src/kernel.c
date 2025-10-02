//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "gdt.h"
#include "interrupts.h"
#include "memory.h"

void kernel_main(void) {
    memory_heap_init();
    gdt_init();
    interrupts_init();

    screen_clear_screen();
    const char* msgs[] = { "Eitan OS Started...\n", "Hello user!\n" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i], strlen(msgs[i]));

    // for (int i = 0; i < 30; i++) {
    //     char* r = num_to_str(rand() % 100 + 1);
    //     char* r2 = str_concat(r, "\n");
    //     screen_print(r2, strlen(r2));
    //     free(r);
    //     free(r2);
    // }

    while (1) {
        const char keyboard = io_keyboard_read();
        screen_print(&keyboard, 1);
    }

    while (1) {
        asm volatile("hlt");
    }
}
