//
// Created by eitan on 9/20/25.
//

#include "screen.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile unsigned short* VGA = (unsigned short*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

void screen_put_char(char c, int x, int y) {
    VGA[y * VGA_WIDTH + x] = 0x0F00 | c;
}

char screen_get_char(int x, int y) {
    return VGA[y * VGA_WIDTH + x] & 0xFF;
}

void screen_norm_cursor() {
    if(cursor_x < 0) {
        cursor_y--;
        cursor_x = 0;
        while (screen_get_char(cursor_x, cursor_y) != 0)
            cursor_x++;
        cursor_x--;
    }
    if(cursor_x >= VGA_WIDTH){
        cursor_x = 0;
        cursor_y++;
    }
}

void screen_print(const char* msg, int size) {
    for (int i = 0; i < size; i++) {
        if (msg[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else if (msg[i] == 8) {
            cursor_x--;
            screen_norm_cursor();
            screen_put_char(0, cursor_x, cursor_y);
        } else {
            screen_put_char(msg[i], cursor_x, cursor_y);
            cursor_x++;
            screen_norm_cursor();
        }
    }
}

void screen_clear_screen() {
    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA[i] = 0;
    }
}
