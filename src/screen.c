//
// Created by eitan on 5/14/26.
//

#include "screen.h"
#include "compiled_fonts/zap.h"

static struct limine_framebuffer* fb = null;
static uint8_t* current_font = null;
static bool_t is_current_font_psf2 = false;

void screen_init(struct limine_framebuffer* framebuffer) {
    fb = framebuffer;
    // screen_clear();
    screen_load_font(zap_font_get());
    // screen_put_char('X', 0, 0, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
    screen_print("Eitan");
}
w
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

#define BUFFER_SIZE 4096

static int cursor_x = 0;
static int cursor_y = 0;

static char buffer[BUFFER_SIZE] = {};
static int scroll = 0;
static int buffer_end = 0;

void screen_norm_cursor() {
    if (cursor_x < 0) {
        cursor_y--;
        cursor_x = 0;
        while (buffer[cursor_x + cursor_y * fb->width / 8] != 0) // FIX
            cursor_x++;
        cursor_x--;
    }
    if (cursor_x >= fb->width / 8) { // FIX
        cursor_x = 0;
        cursor_y++;
    }
}

int amin(int a, int b) {
    return a < b ? a : b;
}
int amax(int a, int b) {
    return a > b ? a : b;
}

void screen_flush() {
    int buffer_start = 0;
    if (scroll != 0) {
        int new_line_count = 0;
        bool_t counted_enough = 0;
        for (int i = 0; i < buffer_end; i++) {
            if (buffer[i] == '\n') {
                new_line_count++;

                if (new_line_count >= scroll) {
                    buffer_start = i + 1;
                    counted_enough = 1;
                    break;
                }
            }
        }

        if (!counted_enough) {
            for(int y = 0; y < fb->height; y++) {
                for(int x = 0; x < fb->width; x++) {
                    screen_put_pixel(x, y, screen_make_color(0, 0, 0));
                }
            }
            return;
        }
    }
    int buffer_end_clamped = amin(buffer_end, buffer_start + fb->width * fb->height);
    cursor_x = 0;
    cursor_y = 0;
    for(int y = 0; y < fb->height; y++) {
        for(int x = 0; x < fb->width; x++) {
            screen_put_pixel(x, y, screen_make_color(0, 0, 0));
        }
    }

    for (int i = buffer_start; i < buffer_end_clamped; i++) {
        if (buffer[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else if (buffer[i] == 8) {
            cursor_x--;
            screen_norm_cursor();
        } else {
            screen_put_char(buffer[i], cursor_x, cursor_y, screen_make_color(255, 255, 255), screen_make_color(0, 0, 0));
            cursor_x++;
            screen_norm_cursor();
        }
    }
}

int astrlen(const char* str) {
    int count = 0;
    while (str[count] != '\0') {
        if (count > 1000)
            return -1;
        count++;
    }
    return count;
}

void* amemcpy(void* dest, const void* src, const size_t size) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* amemset(void* dest, uint8_t val, size_t size) {
    unsigned char *d = dest;
    while (size--) {
        *d++ = val;
    }
    return dest;
}

void screen_print(const char* msg) {
    int size = amin(astrlen(msg), BUFFER_SIZE - buffer_end);
    amemcpy(&buffer[buffer_end], msg, size);
    buffer_end += size;

    screen_flush();
}

void screen_scroll(int to) {
    scroll = amax(to, 0);
    screen_flush();
}

int screen_get_scroll() {
    return scroll;
}

void screen_clear() {
    amemset(buffer, 0, BUFFER_SIZE);
    buffer_end = 0;
    scroll = 0;
    screen_flush();
}