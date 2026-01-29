//
// Created by eitan on 9/20/25.
//

#include "screen.h"

#include "eitan_lib.h"
#include "memory.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define BUFFER_SIZE (VGA_WIDTH * VGA_HEIGHT * 3)

volatile uint16_t* VGA = (uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

char buffer[BUFFER_SIZE] = {};
int scroll = 0;
int buffer_end = 0;

void screen_put_char(char c, int x, int y) {
    if (cursor_y >= VGA_HEIGHT || cursor_x >= VGA_WIDTH)
        return;

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

void screen_flush() {
    int buffer_start = 0;
    if (scroll != 0) {
        int new_line_count = 0;
        bool_t counted_enough = 0;
        for (int i = 0; i < buffer_end; i++) {
            if (buffer[i] == '\n') {
                new_line_count++;

                if (new_line_count >= scroll) {
                    buffer_start = i + 1;
                    counted_enough = 1;
                    break;
                }
            }
        }

        if (!counted_enough) {
            screen_clear_screen();
            return;
        }
    }
    int buffer_end_clamped = min(buffer_end, buffer_start + VGA_WIDTH * VGA_HEIGHT);
    cursor_x = 0;
    cursor_y = 0;
    screen_clear_screen();

    for (int i = buffer_start; i < buffer_end_clamped; i++) {
        if (buffer[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else if (buffer[i] == 8) {
            cursor_x--;
            screen_norm_cursor();
            screen_put_char(0, cursor_x, cursor_y);
        } else {
            screen_put_char(buffer[i], cursor_x, cursor_y);
            cursor_x++;
            screen_norm_cursor();
        }
    }
}

void screen_print(const char* msg) {
    int size = min(strlen(msg), BUFFER_SIZE - buffer_end);
    memcpy(&buffer[buffer_end], msg, size);
    buffer_end += size;

    screen_flush();
}

void screen_println(const char* msg) {
    char* new_msg = str_concat(msg, "\n");
    screen_print(new_msg);
    free(new_msg);
}

void screen_print_num(double num) {
    char* msg = num_to_str(num);
    screen_print(msg);
    free(msg);
}

void screen_println_num(double num) {
    char* msg = num_to_str(num);
    screen_println(msg);
    free(msg);
}

void screen_print_arr(const char** msgs, int count) {
    char* msg = str_concats(msgs, count);
    screen_print(msg);
    free(msg);
}

void screen_println_arr(const char** msgs, int count) {
    char* msg = str_concats(msgs, count);
    screen_println(msg);
    free(msg);
}

void screen_print_arr_num(const double* nums, int count) {
    char* msgs[count];
    for (int i = 0; i < count; i++) {
        msgs[i] = num_to_str(nums[i]);
    }

    char* msg = str_concats(msgs, count);
    screen_print(msg);

    free(msg);
    for (int i = 0; i < count; i++) {
        free(msgs[i]);
    }
}

void screen_println_arr_num(const double* nums, int count) {
    char* msgs[count];
    for (int i = 0; i < count; i++) {
        msgs[i] = num_to_str(nums[i]);
    }

    char* msg = str_concats(msgs, count);
    screen_println(msg);

    free(msg);
    for (int i = 0; i < count; i++) {
        free(msgs[i]);
    }
}

void screen_scroll(int to) {
    scroll = max(to, 0);
    screen_flush();
}

int screen_get_scroll() {
    return scroll;
}

void screen_clear_screen() {
    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA[i] = 0;
    }
}
