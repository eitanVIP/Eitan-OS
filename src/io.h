//
// Created by eitan on 9/20/25.
//

#pragma once

#include "stdint.h"

void io_outb(unsigned short port, unsigned char val);
unsigned char io_inb(unsigned short port);
void io_outw(unsigned short port, unsigned short val);
unsigned short io_inw(unsigned short port);

uint16_t io_keyboard_read();
bool_t io_is_character(uint16_t scancode);
char io_scancode_to_character(uint16_t scancode);