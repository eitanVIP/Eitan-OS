//
// Created by eitan on 5/14/26.
//

#include "screen.h"
#include "compiled_fonts/zap.h"
#include "eitan_lib.h"

static struct limine_framebuffer* fb = null;
static uint8_t* current_font = null;
static bool_t is_current_font_psf2 = false;

void screen_init(struct limine_framebuffer* framebuffer) {
    fb = framebuffer;
    screen_load_font(zap_font_get());
    screen_clear();
    screen_print("[screen] screen init\n");
}

void screen_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    uint32_t *row = (uint32_t *)((uint8_t *)fb->address + y * fb->pitch);
    row[x] = color;
}

uint32_t screen_make_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << fb->red_mask_shift)
         | ((uint32_t)g << fb->green_mask_shift)
         | ((uint32_t)b << fb->blue_mask_shift);
}

uint64_t screen_get_width() {
    return fb->width;
}

uint64_t screen_get_height() {
    return fb->height;
}

void screen_load_font(uint8_t* font) {
    is_current_font_psf2 = false;

    if (((PSF2_header*)font)->magic == 0x864ab572) {
        current_font = font;
        is_current_font_psf2 = true;
    }
    else if (((PSF1_header*)font)->magic == 0x0436)
        current_font = font;
    else
        current_font = null;
}

void screen_put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if (current_font == null)
        return;

    if (is_current_font_psf2) {
        uint8_t* glyph = current_font + ((PSF2_header*)current_font)->header_size + c * ((PSF2_header*)current_font)->bytes_per_glyph;

        x *= ((PSF2_header*)current_font)->width;
        y *= ((PSF2_header*)current_font)->height;

        for (uint32_t row = 0; row < ((PSF2_header*)current_font)->height; row++) {
            for (uint32_t col = 0; col < ((PSF2_header*)current_font)->width; col++) {
                uint8_t byte = glyph[row * ((((PSF2_header*)current_font)->width + 7) / 8)];
                uint32_t color = (byte >> (7 - col)) & 1 ? fg_color : bg_color;
                screen_put_pixel(x + col, y + row, color);
            }
        }
    } else {
        uint8_t* glyph = current_font + sizeof(PSF1_header) + c * ((PSF1_header*)current_font)->charsize;

        x *= 8;
        y *= ((PSF1_header*)current_font)->charsize;

        for (uint32_t row = 0; row < ((PSF1_header*)current_font)->charsize; row++) {
            for (uint32_t col = 0; col < 8; col++) {
                uint32_t color = (glyph[row] >> (7 - col)) & 1 ? fg_color : bg_color;
                screen_put_pixel(x + col, y + row, color);
            }
        }
    }
}

uint32_t screen_get_font_char_width() {
    if (current_font == null)
        return 0;

    if (is_current_font_psf2)
        return ((PSF2_header*)current_font)->width;
    else
        return 8;
}

uint32_t screen_get_font_char_height() {
    if (current_font == null)
        return 0;

    if (is_current_font_psf2)
        return ((PSF2_header*)current_font)->height;
    else
        return ((PSF1_header*)current_font)->charsize;
}

#define BUFFER_SIZE 4096

static int cursor_x = 0;
static int cursor_y = 0;

static char buffer[BUFFER_SIZE] = {};
static int scroll = 0;
static int buffer_end = 0;

void screen_norm_cursor() {
    uint64_t width = fb->width / screen_get_font_char_width();
    uint64_t height = fb->height / screen_get_font_char_height();

    if (cursor_x < 0) {
        cursor_y--;
        cursor_x = 0;
        while (buffer[cursor_x + cursor_y * width] != 0)
            cursor_x++;
        cursor_x--;
    }
    if (cursor_x >= width) {
        cursor_x = 0;
        cursor_y++;
    }
}

void screen_flush() {
    uint64_t width = fb->width / screen_get_font_char_width();
    uint64_t height = fb->height / screen_get_font_char_height();

    int buffer_start = 0;
    if (scroll > 0) {
        // int new_line_count = 0;
        // bool_t counted_enough = 0;
        // for (int i = 0; i < buffer_end; i++) {
        //     if (buffer[i] == '\n') {
        //         new_line_count++;
        //
        //         if (new_line_count >= scroll) {
        //             buffer_start = i + 1;
        //             counted_enough = 1;
        //             break;
        //         }
        //     }
        // }
        //
        // if (!counted_enough) {
        //     for(int y = 0; y < fb->height; y++) {
        //         for(int x = 0; x < fb->width; x++) {
        //             screen_put_pixel(x, y, screen_make_color(0, 0, 0));
        //         }
        //     }
        //     return;
        // }

        buffer_start = scroll * width;
        if (buffer_start >= BUFFER_SIZE) {
            for(int y = 0; y < fb->height; y++) {
                for(int x = 0; x < fb->width; x++) {
                    screen_put_pixel(x, y, screen_make_color(0, 0, 0));
                }
            }
            return;
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    for(int y = 0; y < fb->height; y++) {
        for(int x = 0; x < fb->width; x++) {
            screen_put_pixel(x, y, screen_make_color(0, 0, 0));
        }
    }

    int buffer_end_clamped = min(buffer_end, buffer_start + width * height);
    for (int i = buffer_start; i < buffer_end_clamped; i++) {
        // if (buffer[i] == '\n') {
        //     cursor_x = 0;
        //     cursor_y++;
        // } else if (buffer[i] == 8) { // backspace
        //     screen_put_char(' ', cursor_x, cursor_y, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
        //     cursor_x--;
        //     screen_norm_cursor();
        // } else {
        //     screen_put_char(buffer[i], cursor_x, cursor_y, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
        //     cursor_x++;
        //     screen_norm_cursor();
        // }

        if (buffer[i] == 0) {
            screen_put_char(' ', cursor_x, cursor_y, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
            cursor_x++;
            screen_norm_cursor();
        } else {
            screen_put_char(buffer[i], cursor_x, cursor_y, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
            cursor_x++;
            screen_norm_cursor();
        }
    }
}

void screen_print(const char* msg) {
    uint64_t width = fb->width / screen_get_font_char_width();

    for (int i = 0; msg[i] != '\0'; i++) {
        // Prevent buffer overflow
        if (buffer_end >= BUFFER_SIZE) {
            screen_flush();
            buffer_end = 0;
        }

        char c = msg[i];

        if (c == '\n') {
            uint64_t current_col = buffer_end % width;
            buffer_end += (width - current_col);
        }
        else if (c == '\b') {
            if (buffer_end > 0) {
                buffer[buffer_end] = 0;
                buffer_end--;
            }
        }
        else if (c == '\r') {
            buffer_end -= (buffer_end % width);
        }
        else if (c == '\t') {
            int tab_size = 4;
            int spaces_to_add = tab_size - (buffer_end % tab_size);
            for (int s = 0; s < spaces_to_add && buffer_end < BUFFER_SIZE; s++) {
                buffer[buffer_end++] = ' ';
            }
        }
        else {
            buffer[buffer_end++] = c;
        }
    }

    screen_flush();
}

void screen_scroll(int to) {
    scroll = max(to, 0);
    screen_flush();
}

int screen_get_scroll() {
    return scroll;
}

void screen_clear() {
    memset(buffer, 0, BUFFER_SIZE);
    buffer_end = 0;
    scroll = 0;
    screen_flush();
}