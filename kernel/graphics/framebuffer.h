#pragma once
#include "../stdint.h"

typedef struct {
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint32_t *backbuf;
} framebuffer_t;

extern framebuffer_t fb;

void fb_init(uint64_t addr, uint32_t pitch, uint32_t width,
             uint32_t height, uint8_t bpp, void *backbuf);
void fb_putpixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_hline(int x, int y, int w, uint32_t color);
void fb_draw_vline(int x, int y, int h, uint32_t color);
void fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void fb_clear(uint32_t color);
void fb_fill_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void fb_fill_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right);
void fb_flip(void);
void fb_flip_rect(int x, int y, int w, int h);
void fb_save_region(int x, int y, int w, int h, uint32_t *out);
void fb_restore_region(int x, int y, int w, int h, const uint32_t *in);

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
           ((uint32_t)g << 8) | (uint32_t)b;
}
static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t alpha) {
    uint8_t r = ((src>>16)&0xFF)*alpha/255 + ((dst>>16)&0xFF)*(255-alpha)/255;
    uint8_t g = ((src>> 8)&0xFF)*alpha/255 + ((dst>> 8)&0xFF)*(255-alpha)/255;
    uint8_t b = ((src    )&0xFF)*alpha/255 + ((dst    )&0xFF)*(255-alpha)/255;
    return rgb(r,g,b);
}
