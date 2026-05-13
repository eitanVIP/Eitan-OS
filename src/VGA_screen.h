//
// Created by eitan on 9/20/25.
//

#pragma once

void VGA_screen_put_char(char c, int x, int y);
char VGA_screen_get_char(int x, int y);
void VGA_screen_flush();

void VGA_screen_print(const char* msg);
void VGA_screen_println(const char* msg);
void VGA_screen_print_num(double num);
void VGA_screen_println_num(double num);
void VGA_screen_print_arr(const char** msgs, int count);
void VGA_screen_println_arr(const char** msgs, int count);
void VGA_screen_print_arr_num(const double* nums, int count);
void VGA_screen_println_arr_num(const double* nums, int count);

void VGA_screen_scroll(int to);
int VGA_screen_get_scroll();
void VGA_screen_clear_screen();