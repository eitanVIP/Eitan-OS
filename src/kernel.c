//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"

int strlen(const char* str) {
    int count = 0;
    while (str[count] != '\0') {
        if (count > 1000)
            return -1;
        count++;
    }
    return count;
}

void kernel_main(void) {
    screen_clear_screen();
    const char* msgs[] = { "Eitan OS Started...\n\0", "Hello user!\n\0" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i], strlen(msgs[i]));

    while (1) {
        const char keyboard = io_keyboard_read();
        screen_print(&keyboard, 1);
    }

    while (1) {
        asm volatile("hlt");
    }
}
