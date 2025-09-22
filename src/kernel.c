//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "memory.h"

void kernel_main(void) {
    memory_heap_init();

    screen_clear_screen();
    const char* msgs[] = { "Eitan OS Started...\n", "Hello user!\n" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i], strlen(msgs[i]));

    char* num1 = num_to_str(5);
    char* num1F = str_concat(num1, "\n");
    char* num2 = num_to_str(0.8);
    char* num2F = str_concat(num2, "\n");
    char* num3 = num_to_str(-1.2);
    char* num3F = str_concat(num3, "\n");
    char* num4 = num_to_str(-12.345);
    char* num4F = str_concat(num4, "\n");
    screen_print(num1F, strlen(num1F));
    screen_print(num2F, strlen(num2F));
    screen_print(num3F, strlen(num3F));
    screen_print(num4F, strlen(num4F));
    free(num1);
    free(num2);
    free(num3);
    free(num4);
    free(num1F);
    free(num2F);
    free(num3F);
    free(num4F);

    while (1) {
        const char keyboard = io_keyboard_read();
        screen_print(&keyboard, 1);
    }

    while (1) {
        asm volatile("hlt");
    }
}
