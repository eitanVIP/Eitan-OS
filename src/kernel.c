//
// Created by eitan on 9/19/25.
//

#include "VGA_screen.h"
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
#include "pmm.h"
#include "screen.h"

extern void enable_sse(void);

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
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
     || framebuffer_request.response->framebuffer_count < 1
     || memmap_request.response == null
     || hhdm_request.response == null) {
        while (1) {
            asm volatile("hlt");
        }
     }

    enable_sse();
    screen_init(framebuffer_request.response->framebuffers[0]);
    gdt_init();
    pmm_init(&memmap_request, &hhdm_request);
    // process_scheduler_init();
    // interrupts_init();

    screen_print("[kernel] Eitan OS Started...\n");
    screen_print("[kernel] Hello user!\n");

    // filesystem_init();

    // uint32_t pid;
    // program_loader_load_elf32(shell_program_get(), &pid);

    // while (1) {
    //     uint16_t scancode = io_keyboard_read();
    //     io_keyboard_buffer = scancode;
    //
    //     if (!io_is_character(scancode)) {
    //         switch (scancode & 0xFF) {
    //             case 0x48: // Arrow Up
    //                 screen_scroll(screen_get_scroll() - 1);
    //                 break;
    //
    //             case 0x50: // Arrow Down
    //                 screen_scroll(screen_get_scroll() + 1);
    //                 break;
    //
    //             case 0x58: // F12
    //                 // screen_println("TEST");
    //                 process_scheduler_remove_process(pid);
    //                 break;
    //
    //             default:
    //                 break;
    //         }
    //     }
    // }

    while (1) {
        asm volatile("hlt");
    }
}
