//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "filesystem.h"
#include "gdt.h"
#include "interrupts.h"
#include "memory.h"
#include "process_scheduler.h"

void multi_process_test(void) {
    unsigned char* test_code = malloc(64);
    unsigned char* print_func = screen_print;
    unsigned int relative_jmp = (int)print_func - ((int)test_code + 24);
    // mov ebp, esp
    test_code[0]  = 0x89;
    test_code[1]  = 0xE5;
    // sub esp, 4
    test_code[2]  = 0x83;
    test_code[3]  = 0xEC;
    test_code[4]  = 0x04;
    // mov byte [ebp-4], 0x61
    test_code[5]  = 0xC6;
    test_code[6]  = 0x45;
    test_code[7]  = 0xFC;
    test_code[8]  = 0x61;
    // mov eax, 1
    test_code[9] = 0xB8;
    test_code[10] = 0x01;
    test_code[11] = 0x00;
    test_code[12] = 0x00;
    test_code[13] = 0x00;
    // push eax
    test_code[14] = 0x50;
    // lea eax, [ebp-4]
    test_code[15]  = 0x8D;
    test_code[16] = 0x45;
    test_code[17] = 0xFC;
    // push eax
    test_code[18] = 0x50;

    // CALL
    test_code[19] = 0xE8;
    test_code[20] = relative_jmp >> (0 * 8) & 0xFF;
    test_code[21] = relative_jmp >> (1 * 8) & 0xFF;
    test_code[22] = relative_jmp >> (2 * 8) & 0xFF;
    test_code[23] = relative_jmp >> (3 * 8) & 0xFF;

    test_code[24] = 0x83;
    test_code[25] = 0xC4;
    test_code[26] = 0x08;

    // LOOP
    test_code[27] = 0xB9; // mov ecx...
    test_code[28] = 0x00;
    test_code[29] = 0x00;
    test_code[30] = 0x00;
    test_code[31] = 0x00;

    test_code[32] = 0x41; // inc ecx

    test_code[33] = 0x81; // cmp ecx, X
    test_code[34] = 0xF9;
    test_code[35] = 0x00;
    test_code[36] = 0x00;
    test_code[37] = 0x00;
    test_code[38] = 0x10;

    test_code[39] = 0x75;
    test_code[40] = 0xF7;

    // JUMP
    test_code[41] = 0xEB;
    test_code[42] = 0xD5;
    process_scheduler_add_process(test_code);
}

void kernel_main(void) {
    memory_heap_init();
    gdt_init();
    process_scheduler_init();
    interrupts_init();

    screen_clear_screen();
    const char* msgs[] = { "Eitan OS Started...\n", "Hello user!\n" };
    for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
        screen_print(msgs[i]);

    filesystem_init();

    // filesystem_write_sectors(30, "SIGMA", 6);
    char data[6];
    filesystem_read_sectors(30, data, 6);
    screen_print(data);

    // const char* test[] = { "1, ", "2, ", "3, ", "4, ", "5, ", "6, ", "7\n" };
    // const char* msg = str_concats(test, sizeof(test) / sizeof(test[0]));
    // screen_print(msg);
    // memory_print_blocks();

    while (1) {
        const char keyboard[2] = { io_keyboard_read(), '\0' };
        screen_print(keyboard);
    }

    while (1) {
        asm volatile("hlt");
    }
}
