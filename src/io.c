//
// Created by eitan on 9/20/25.
//

#include "io.h"

#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_DATA_PORT 0x60
#define SCAN_SPECIAL_PREFIX 0xE0

void io_outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char io_inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void io_outw(unsigned short port, unsigned short val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

unsigned short io_inw(unsigned short port) {
    unsigned short ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
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

uint16_t io_keyboard_read() {
    uint8_t first_byte;

    // 1. Wait for a key press (Make code, not Break code)
    do {
        while (!(io_inb(KEYBOARD_STATUS_PORT) & 1)); // Wait for data
        first_byte = io_inb(KEYBOARD_DATA_PORT);
    } while (first_byte & 0x80); // Skip release codes

    // 2. Check if it is a "Special" prefix
    if (first_byte == SCAN_SPECIAL_PREFIX) {
        // Wait for the actual arrow/special byte
        while (!(io_inb(KEYBOARD_STATUS_PORT) & 1));
        unsigned char second_byte = io_inb(KEYBOARD_DATA_PORT);

        return first_byte << 8 | second_byte;
    }

    // 3. Otherwise, it's a normal key
    return first_byte;
}

bool_t io_is_character(uint16_t scancode) {
    return scancode >> 8 == SCAN_SPECIAL_PREFIX;
}

char io_scancode_to_character(uint16_t scancode) {
    return scancode_to_ascii[scancode];
}