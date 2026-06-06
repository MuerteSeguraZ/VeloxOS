#pragma once
#include "../stdint.h"

#define TEXT_SCALE 1
#define GLYPH_W (8 * TEXT_SCALE)
#define GLYPH_H (8 * TEXT_SCALE)

void text_putchar(int x, int y, char c, uint32_t fg, uint32_t bg, int transparent_bg);
void text_puts(int x, int y, const char *s, uint32_t fg, uint32_t bg, int transparent_bg);
int  text_strlen(const char *s);
void text_puts_centered(int cx, int y, int total_w, const char *s,
                        uint32_t fg, uint32_t bg, int transparent_bg);
