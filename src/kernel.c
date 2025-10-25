//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "gdt.h"
#include "interrupts.h"
#include "memory.h"
#include "process_scheduler.h"

void kernel_main(void) {
    memory_heap_init();
    gdt_init();
    process_scheduler_init();
    interrupts_init();
    screen_clear_screen();

    const char* msgs[] = { "Eitan OS Started...\n", "Hello user!\n" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i], strlen(msgs[i]));

    // while (1) {
    //     const char keyboard = io_keyboard_read();
    //     screen_print(&keyboard, 1);
    // }

    unsigned char* test_code = malloc(16);
    test_code[0] = 0xEB;
    test_code[1] = 0xFE;
    for (int i = 2; i < 16; i++) test_code[i] = 0x90;
    process_scheduler_add_process(test_code);

    while (1) {
        asm volatile("hlt");
    }
}
