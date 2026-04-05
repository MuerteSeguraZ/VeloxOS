#include "text.h"
#include "font.h"
#include "framebuffer.h"

void text_putchar(int x, int y, char c, uint32_t fg, uint32_t bg, int transparent_bg) {
    if ((unsigned char)c < 32 || (unsigned char)c > 127) c = '?';
    const uint8_t *glyph = font8x8[(uint8_t)c - 32];

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col * TEXT_SCALE;
            int py = y + row * TEXT_SCALE;
            if (bits & (1 << col)) {
                fb_fill_rect(px, py, TEXT_SCALE, TEXT_SCALE, fg);
            } else if (!transparent_bg) {
                fb_fill_rect(px, py, TEXT_SCALE, TEXT_SCALE, bg);
            }
        }
    }
}

void text_puts(int x, int y, const char *s, uint32_t fg, uint32_t bg, int transparent_bg) {
    int cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += GLYPH_H + 2;
        } else {
            text_putchar(cx, y, *s, fg, bg, transparent_bg);
            cx += GLYPH_W + 1;
        }
        s++;
    }
}

int text_strlen(const char *s) {
    int n = 0;
    while (*s++) n++;
    return n;
}

void text_puts_centered(int cx, int y, int total_w, const char *s,
                        uint32_t fg, uint32_t bg, int transparent_bg) {
    int pw = text_strlen(s) * (GLYPH_W + 1);
    int x  = cx + (total_w - pw) / 2;
    text_puts(x, y, s, fg, bg, transparent_bg);
}
