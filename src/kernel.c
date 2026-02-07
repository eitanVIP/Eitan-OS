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
#include "program_loader.h"
#include "compiled_programs/shell.h"
#include "compiled_programs/test.h"

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

    uint32_t pid;
    // program_loader_load_elf32(test_program_get(), &pid);
    program_loader_load_elf32(shell_program_get(), &pid);

    while (1) {
        uint16_t scancode = io_keyboard_read();
        if (io_is_character(scancode)) {
            const char keyboard[2] = { io_scancode_to_character(scancode), '\0' };
            // screen_print(keyboard);
        } else {
            switch (scancode & 0xFF) {
                case 0x48: // Arrow Up
                    screen_scroll(screen_get_scroll() - 1);
                    break;

                case 0x50: // Arrow Down
                    screen_scroll(screen_get_scroll() + 1);
                    break;

                case 0x58: // F12
                    // screen_println("TEST");
                    process_scheduler_remove_process(pid);
                    break;

                default:
                    break;
            }
        }
    }

    while (1) {
        asm volatile("hlt");
    }
}
