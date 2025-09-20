//
// Created by eitan on 9/20/25.
//

#pragma once

static inline void io_outb(unsigned short port, unsigned char val);
static inline unsigned char io_inb(unsigned short port);

unsigned char io_keyboard_get();
char io_keyboard_read();