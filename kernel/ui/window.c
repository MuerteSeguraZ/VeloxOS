#include "window.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

// ── Color palette ─────────────────────────────────────────────────────────────
#define COL_WIN_TITLEBAR_ACT  0x1e3a5f
#define COL_WIN_TITLEBAR_INACT 0x1a1a2e
#define COL_WIN_TITLEBAR_GRAD  0x0d2137
#define COL_WIN_BODY          0x12121e
#define COL_WIN_BODY2         0x1a1a2e
#define COL_WIN_BORDER_ACT    0x4a7fa5
#define COL_WIN_BORDER_INACT  0x2a2a4a
#define COL_WIN_SHADOW        0x000000
#define COL_TEXT_WHITE        0xf0f0f0
#define COL_TEXT_DIM          0x7080a0
#define COL_BTN_CLOSE         0xe05050
#define COL_BTN_MIN           0xe0a030
#define COL_BTN_MAX           0x40b060
#define COL_BTN_CLOSE_H       0xff6060
#define COL_BTN_RIM           0x00000040

void window_draw(window_t *win, int active) {
    if (!win->visible) return;

    int x = win->x, y = win->y, w = win->w, h = win->h;
    uint32_t border = active ? COL_WIN_BORDER_ACT : COL_WIN_BORDER_INACT;

    // Drop shadow
    fb_fill_rect(x + 5, y + 5, w, h, 0x000000);

    // Outer border
    fb_fill_rect(x - WINDOW_BORDER, y - WINDOW_BORDER,
                 w + WINDOW_BORDER*2, h + WINDOW_BORDER*2, border);

    // Titlebar gradient
    if (active) {
        fb_fill_gradient_v(x, y, w, TITLEBAR_HEIGHT,
                           COL_WIN_TITLEBAR_ACT, COL_WIN_TITLEBAR_GRAD);
    } else {
        fb_fill_rect(x, y, w, TITLEBAR_HEIGHT, COL_WIN_TITLEBAR_INACT);
    }

    // Titlebar bottom separator
    fb_draw_hline(x, y + TITLEBAR_HEIGHT - 1, w, border);

    // Window body
    fb_fill_gradient_v(x, y + TITLEBAR_HEIGHT,
                       w, h - TITLEBAR_HEIGHT,
                       COL_WIN_BODY, COL_WIN_BODY2);

    // ── Traffic light buttons ─────────────────────────────────────────────────
    int by = y + TITLEBAR_HEIGHT / 2 - 5;

    // Close (red)
    fb_fill_rect(x + 8,  by, 10, 10, active ? COL_BTN_CLOSE : 0x444444);
    fb_draw_rect(x + 8,  by, 10, 10, 0x00000030);

    // Minimise (yellow)
    fb_fill_rect(x + 22, by, 10, 10, active ? COL_BTN_MIN : 0x444444);
    fb_draw_rect(x + 22, by, 10, 10, 0x00000030);

    // Maximise (green)
    fb_fill_rect(x + 36, by, 10, 10, active ? COL_BTN_MAX : 0x444444);
    fb_draw_rect(x + 36, by, 10, 10, 0x00000030);

    // Title — centered
    int title_len = text_strlen(win->title);
    int title_px  = title_len * (GLYPH_W + 1);
    int title_x   = x + (w - title_px) / 2;
    int title_y   = y + (TITLEBAR_HEIGHT - GLYPH_H) / 2;
    text_puts(title_x, title_y, win->title,
              active ? COL_TEXT_WHITE : COL_TEXT_DIM, 0, 1);

    // ── Body content ──────────────────────────────────────────────────────────
    int cy = y + TITLEBAR_HEIGHT + 14;
    text_puts(x + 14, cy,      win->title,       0x6090c0, 0, 1);
    text_puts(x + 14, cy + 16, "Velox OS v0.something",  COL_TEXT_DIM, 0, 1);
    // Decorative divider
    fb_draw_hline(x + 14, cy + 30, w - 28, 0x2a3a5a);
    text_puts(x + 14, cy + 38, "Window compositor working", COL_TEXT_DIM, 0, 1);
    text_puts(x + 14, cy + 54, "PS/2 mouse + keyboard.", COL_TEXT_DIM, 0, 1);
    text_puts(x + 14, cy + 70, "PIT timer @ 60hz.", COL_TEXT_DIM, 0, 1);
}

int window_hit_button(window_t *win, int mx, int my) {
    if (!win->visible) return 0;
    int by = win->y + TITLEBAR_HEIGHT / 2 - 5;
    if (my < by || my >= by + 10) return 0;
    if (mx >= win->x + 8  && mx < win->x + 18)  return BTN_CLOSE;
    if (mx >= win->x + 22 && mx < win->x + 32)  return BTN_MIN;
    if (mx >= win->x + 36 && mx < win->x + 46)  return BTN_MAX;
    return 0;
}
