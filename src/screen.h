//
// Created by eitan on 9/20/25.
//

#pragma once

void screen_put_char(char c, int x, int y);
char screen_get_char(int x, int y);
void screen_flush();
void screen_print(const char* msg);
void screen_println(const char* msg);
void screen_scroll(int to);
int screen_get_scroll();
void screen_clear_screen();