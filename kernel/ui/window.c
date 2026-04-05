#include "window.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

#define COL_WIN_TITLEBAR_ACT   0x1e3a5f
#define COL_WIN_TITLEBAR_INACT 0x1a1a2e
#define COL_WIN_TITLEBAR_GRAD  0x0d2137
#define COL_WIN_BODY           0x12121e
#define COL_WIN_BODY2          0x1a1a2e
#define COL_WIN_BORDER_ACT     0x4a7fa5
#define COL_WIN_BORDER_INACT   0x2a2a4a
#define COL_TEXT_WHITE         0xf0f0f0
#define COL_TEXT_DIM           0x7080a0
#define COL_TEXT_CONTENT       0xc8d8e8
#define COL_BTN_CLOSE          0xe05050
#define COL_BTN_MIN            0xe0a030
#define COL_BTN_MAX            0x40b060

void window_set_content(window_t *win, const char *text, uint32_t len) {
    if (len >= WIN_CONTENT_MAX) len = WIN_CONTENT_MAX - 1;
    for (uint32_t i = 0; i < len; i++) win->content[i] = text[i];
    win->content[len] = 0;
    win->has_content = 1;
}

void window_draw(window_t *win, int active) {
    if (!win->visible) return;

    int x = win->x, y = win->y, w = win->w, h = win->h;
    uint32_t border = active ? COL_WIN_BORDER_ACT : COL_WIN_BORDER_INACT;

    // Shadow
    fb_fill_rect(x + 5, y + 5, w, h, 0x000000);

    // Border
    fb_fill_rect(x - WINDOW_BORDER, y - WINDOW_BORDER,
                 w + WINDOW_BORDER*2, h + WINDOW_BORDER*2, border);

    // Titlebar
    if (active)
        fb_fill_gradient_v(x, y, w, TITLEBAR_HEIGHT,
                           COL_WIN_TITLEBAR_ACT, COL_WIN_TITLEBAR_GRAD);
    else
        fb_fill_rect(x, y, w, TITLEBAR_HEIGHT, COL_WIN_TITLEBAR_INACT);

    fb_draw_hline(x, y + TITLEBAR_HEIGHT - 1, w, border);

    // Body
    fb_fill_gradient_v(x, y + TITLEBAR_HEIGHT,
                       w, h - TITLEBAR_HEIGHT,
                       COL_WIN_BODY, COL_WIN_BODY2);

    // Traffic light buttons
    int by = y + TITLEBAR_HEIGHT / 2 - 5;
    fb_fill_rect(x + 8,  by, 10, 10, active ? COL_BTN_CLOSE : 0x444444);
    fb_draw_rect(x + 8,  by, 10, 10, 0x00000030);
    fb_fill_rect(x + 22, by, 10, 10, active ? COL_BTN_MIN   : 0x444444);
    fb_draw_rect(x + 22, by, 10, 10, 0x00000030);
    fb_fill_rect(x + 36, by, 10, 10, active ? COL_BTN_MAX   : 0x444444);
    fb_draw_rect(x + 36, by, 10, 10, 0x00000030);

    // Title centered
    int title_px = text_strlen(win->title) * (GLYPH_W + 1);
    int title_x  = x + (w - title_px) / 2;
    int title_y  = y + (TITLEBAR_HEIGHT - GLYPH_H) / 2;
    text_puts(title_x, title_y, win->title,
              active ? COL_TEXT_WHITE : COL_TEXT_DIM, 0, 1);

    // Body content
    int cx = x + 12;
    int cy = y + TITLEBAR_HEIGHT + 12;
    int max_w = w - 24;
    int max_h = h - TITLEBAR_HEIGHT - 24;

    if (win->has_content) {
        // Render text content with word wrap at character level
        const char *p = win->content;
        int col = 0;
        int max_cols = max_w / (GLYPH_W + 1);
        int max_rows = max_h / (GLYPH_H + 3);
        int row = 0;

        while (*p && row < max_rows) {
            if (*p == '\n') {
                col = 0; row++;
                p++;
                continue;
            }
            if (col >= max_cols) {
                col = 0; row++;
                if (row >= max_rows) break;
            }
            text_putchar(cx + col * (GLYPH_W + 1),
                         cy + row * (GLYPH_H + 3),
                         *p, COL_TEXT_CONTENT, 0, 1);
            col++;
            p++;
        }
    } else {
        // Default placeholder
        text_puts(cx, cy,      win->title,       0x6090c0, 0, 1);
        text_puts(cx, cy + 16, "Velox OS v0.1",  COL_TEXT_DIM, 0, 1);
        fb_draw_hline(cx, cy + 30, max_w, 0x2a3a5a);
        text_puts(cx, cy + 38, "Window compositor active.", COL_TEXT_DIM, 0, 1);
        text_puts(cx, cy + 54, "PS/2 mouse + keyboard.",   COL_TEXT_DIM, 0, 1);
        text_puts(cx, cy + 70, "PIT timer @ 60hz.",        COL_TEXT_DIM, 0, 1);
    }
}

int window_hit_button(window_t *win, int mx, int my) {
    if (!win->visible) return 0;
    int by = win->y + TITLEBAR_HEIGHT / 2 - 5;
    if (my < by || my >= by + 10) return 0;
    if (mx >= win->x + 8  && mx < win->x + 18) return BTN_CLOSE;
    if (mx >= win->x + 22 && mx < win->x + 32) return BTN_MIN;
    if (mx >= win->x + 36 && mx < win->x + 46) return BTN_MAX;
    return 0;
}