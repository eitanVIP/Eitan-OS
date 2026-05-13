//
// Created by eitan on 5/14/26.
//

#ifndef SCREEN_H
#define SCREEN_H

#include "stdint.h"
#include "limine.h"

typedef struct {
    uint16_t magic;   // 0x0436
    uint8_t  mode;
    uint8_t  charsize;
} PSF1_header;

typedef struct {
    uint32_t magic;       // 0x864ab572
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t num_glyphs;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} PSF2_header;

void screen_init(struct limine_framebuffer* framebuffer);
void screen_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t screen_make_color(uint8_t r, uint8_t g, uint8_t b);
uint64_t screen_get_width();
uint64_t screen_get_height();
void screen_load_font(uint8_t* font);
void screen_put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
void screen_norm_cursor();
void screen_flush();
void screen_print(const char* msg);
void screen_scroll(int to);
int screen_get_scroll();
void screen_clear();

#endif //SCREEN_H
