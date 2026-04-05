#include "framebuffer.h"

framebuffer_t fb;

void fb_init(uint64_t addr, uint32_t pitch, uint32_t width,
             uint32_t height, uint8_t bpp, void *backbuf) {
    fb.addr    = addr;
    fb.pitch   = pitch;
    fb.width   = width;
    fb.height  = height;
    fb.bpp     = bpp;
    fb.backbuf = (uint32_t *)backbuf;

    // Clear back buffer
    uint32_t npixels = width * height;
    for (uint32_t i = 0; i < npixels; i++)
        fb.backbuf[i] = 0;
}

// ── Back buffer drawing ───────────────────────────────────────────────────────

static inline int clamp_x(int x) {
    return (x < 0) ? 0 : (x >= (int)fb.width  ? (int)fb.width  - 1 : x);
}
static inline int clamp_y(int y) {
    return (y < 0) ? 0 : (y >= (int)fb.height ? (int)fb.height - 1 : y);
}

void fb_putpixel(int x, int y, uint32_t color) {
    if ((unsigned)x >= fb.width || (unsigned)y >= fb.height) return;
    fb.backbuf[y * fb.width + x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = fb.width  - x;
    if (y + h > (int)fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t *p = fb.backbuf + (y + row) * fb.width + x;
        for (int col = 0; col < w; col++)
            p[col] = color;
    }
}

void fb_draw_hline(int x, int y, int w, uint32_t color) {
    fb_fill_rect(x, y, w, 1, color);
}
void fb_draw_vline(int x, int y, int h, uint32_t color) {
    fb_fill_rect(x, y, 1, h, color);
}
void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_hline(x, y,         w, color);
    fb_draw_hline(x, y + h - 1, w, color);
    fb_draw_vline(x,         y, h, color);
    fb_draw_vline(x + w - 1, y, h, color);
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, fb.width, fb.height, color);
}

static inline uint8_t lerp8(uint8_t a, uint8_t b, int t, int max) {
    return (uint8_t)((int)a + ((int)b - (int)a) * t / max);
}

void fb_fill_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    uint8_t tr=(top>>16)&0xFF, tg=(top>>8)&0xFF, tb=top&0xFF;
    uint8_t br=(bot>>16)&0xFF, bg=(bot>>8)&0xFF, bb=bot&0xFF;
    for (int row = 0; row < h; row++) {
        uint32_t c = rgb(lerp8(tr,br,row,h), lerp8(tg,bg,row,h), lerp8(tb,bb,row,h));
        fb_fill_rect(x, y+row, w, 1, c);
    }
}

void fb_fill_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    uint8_t lr=(left>>16)&0xFF,  lg=(left>>8)&0xFF,  lb=left&0xFF;
    uint8_t rr=(right>>16)&0xFF, rg=(right>>8)&0xFF, rb=right&0xFF;
    for (int col = 0; col < w; col++) {
        uint32_t c = rgb(lerp8(lr,rr,col,w), lerp8(lg,rg,col,w), lerp8(lb,rb,col,w));
        fb_fill_rect(x+col, y, 1, h, c);
    }
}

// ── Blit operations ───────────────────────────────────────────────────────────

void fb_flip(void) {
    uint8_t *screen = (uint8_t *)fb.addr;
    uint8_t *back   = (uint8_t *)fb.backbuf;
    uint32_t row_bytes = fb.width * 4;

    for (uint32_t row = 0; row < fb.height; row++) {
        uint32_t *dst = (uint32_t *)(screen + row * fb.pitch);
        uint32_t *src = (uint32_t *)(back   + row * row_bytes);
        for (uint32_t col = 0; col < fb.width; col++)
            dst[col] = src[col];
    }
}

void fb_flip_rect(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = fb.width  - x;
    if (y + h > (int)fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    uint8_t *screen = (uint8_t *)fb.addr;
    for (int row = 0; row < h; row++) {
        uint32_t *dst = (uint32_t *)(screen + (y+row) * fb.pitch) + x;
        uint32_t *src = fb.backbuf + (y+row) * fb.width + x;
        for (int col = 0; col < w; col++)
            dst[col] = src[col];
    }
}

void fb_save_region(int x, int y, int w, int h, uint32_t *out) {
    if (x < 0 || y < 0 || x+w > (int)fb.width || y+h > (int)fb.height) return;
    for (int row = 0; row < h; row++) {
        uint32_t *src = fb.backbuf + (y+row)*fb.width + x;
        uint32_t *dst = out + row*w;
        for (int col = 0; col < w; col++)
            dst[col] = src[col];
    }
}

void fb_restore_region(int x, int y, int w, int h, const uint32_t *in) {
    if (x < 0 || y < 0 || x+w > (int)fb.width || y+h > (int)fb.height) return;
    for (int row = 0; row < h; row++) {
        uint32_t *dst = fb.backbuf + (y+row)*fb.width + x;
        const uint32_t *src = in + row*w;
        for (int col = 0; col < w; col++)
            dst[col] = src[col];
    }
}
