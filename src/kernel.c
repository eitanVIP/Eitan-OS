//
// Created by eitan on 9/19/25.
//

#include "screen.h"
#include "io.h"
#include "eitan_lib.h"
#include "filesystem.h"
#include "gdt.h"
#include "interrupts.h"
#include "allocator.h"
#include "process_scheduler.h"
#include "program_loader.h"
#include "compiled_programs/shell.h"
#include "compiled_programs/test.h"
#include "limine.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void kernel_main(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        while (1) {
            asm volatile("hlt");
        }
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == null
     || framebuffer_request.response->framebuffer_count < 1) {
        while (1) {
            asm volatile("hlt");
        }
     }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

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
        io_keyboard_buffer = scancode;

        if (!io_is_character(scancode)) {
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
