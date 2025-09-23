//
// Created by eitan on 9/20/25.
//

#include "io.h"

#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_DATA_PORT 0x60

void io_outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char io_inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, // Control
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ','0','.'
};

unsigned char io_keyboard_get() {
    if(io_inb(KEYBOARD_STATUS_PORT) & 1)
        return scancode_to_ascii[io_inb(KEYBOARD_DATA_PORT)];
    return 0;
}

char io_keyboard_read() {
    unsigned char scancode;
    do {
        while (io_keyboard_get() == 0);
        scancode = io_inb(KEYBOARD_DATA_PORT);
    } while (scancode & 0x80);
    return scancode_to_ascii[scancode];
}
