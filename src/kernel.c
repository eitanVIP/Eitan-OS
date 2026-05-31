//
// Created by eitan on 9/19/25.
//

#include "VGA_screen.h"
#include "util/io.h"
#include "util/util.h"
#include "filesystem.h"
#include "gdt.h"
#include "process/interrupts.h"
#include "memory/allocator.h"
#include "process/process_scheduler.h"
#include "process/program_loader.h"
#include "compiled_programs/shell.h"
#include "compiled_programs/test.h"
#include "util/limine.h"
#include "util/panic.h"
#include "memory/pmm.h"
#include "screen.h"
#include "memory/vmm.h"
#include "util/string.h"

extern void enable_sse(void);
extern void enable_nxe(void);

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request kernel_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static uint8_t kernel_stack[65536];  // 64KB
uint8_t* kernel_stack_top = kernel_stack + sizeof(kernel_stack);

void kernel_main(void) {
    asm volatile("mov %0, %%rsp" :: "r"(kernel_stack_top));

    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        panic("limine not supported");
    }
    if (framebuffer_request.response == null
        || framebuffer_request.response->framebuffer_count < 1) {
        panic("invalid limine frame buffer");
    }
    if (memmap_request.response == null) {
        panic("invalid limine memmap");
    }
    if (hhdm_request.response == null) {
        panic("invalid limine hhdmm");
    }

    enable_sse();
    enable_nxe();

    screen_init(framebuffer_request.response->framebuffers[0]);
    gdt_init();

    pmm_init(&memmap_request, &hhdm_request);
    PML4Table* kernel_PML4 = vmm_init(&hhdm_request, &kernel_address_request);
    if (kernel_PML4 == null)
        panic("vmm init crashed");

    // char* buffer[100] = {};
    // char* str = num_to_str_no_malloc((uint64_t)kernel_PML4, buffer, sizeof(buffer));
    // screen_print(str);
    // screen_print("\n");

    process_scheduler_init(kernel_PML4);
    interrupts_init(hhdm_request.response->offset);

    screen_print("[kernel] Eitan OS Started...\n");
    screen_print("[kernel] Hello user!\n");

    // filesystem_init();

    // uint32_t pid;
    // bool_t success = program_loader_load_elf32(shell_program_get(), &pid);
    // if (success)
    //     screen_print("[kernel] Loaded shell\n");
    // else
    //     screen_print("[kernel] Failed to load shell\n");

    uint8_t program[] = "\x48\xB8\xF0\xDE\xBC\x9A\x78\x56\x34\x12\xFF\xE0";
    uint64_t program_ptr = (uint64_t)&program;
    program[2] = program_ptr & 0xFF;
    program[3] = program_ptr >> 8 & 0xFF;
    program[4] = program_ptr >> 16 & 0xFF;
    program[5] = program_ptr >> 24 & 0xFF;
    program[6] = program_ptr >> 32 & 0xFF;
    program[7] = program_ptr >> 40 & 0xFF;
    program[8] = program_ptr >> 48 & 0xFF;
    program[9] = program_ptr >> 56 & 0xFF;

    process_scheduler_add_process(program, true);

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
                    screen_print("TEST\n");
                    // process_scheduler_remove_process(pid);
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
