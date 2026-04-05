#include "desktop.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"
#include "../drivers/rtc.h"

desktop_t desktop;

// ── RNG for star field ────────────────────────────────────────────────────────
static uint32_t rng = 0xdeadbeef;
static uint32_t rng_next(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

// ── Cursor bitmap ─────────────────────────────────────────────────────────────
static const uint8_t cursor_bmp[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,1,1,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
};

// ── Internal draws ────────────────────────────────────────────────────────────

static void draw_wallpaper(void) {
    int dh = fb.height - TASKBAR_HEIGHT;
    fb_fill_gradient_v(0, 0, fb.width, dh, COL_DESKTOP_TOP, COL_DESKTOP_BOT);

    // Subtle horizontal nebula band
    int band_y = dh * 2 / 5;
    int band_h = dh / 5;
    for (int i = 0; i < band_h; i++) {
        uint8_t alpha = (i < band_h/2) ? (i * 40 / (band_h/2))
                                        : ((band_h-i) * 40 / (band_h/2));
        uint32_t c = blend(COL_DESKTOP_BOT, 0x2d1b69, alpha);
        fb_fill_rect(0, band_y + i, fb.width, 1, c);
    }

    // Stars
    rng = 0xdeadbeef;
    for (int i = 0; i < 260; i++) {
        int sx = rng_next() % fb.width;
        int sy = rng_next() % dh;
        uint8_t br = 60 + (rng_next() % 195);
        fb_putpixel(sx, sy, rgb(br, br, br + 20));
    }
    for (int i = 0; i < 20; i++) {
        int sx = rng_next() % fb.width;
        int sy = rng_next() % dh;
        fb_putpixel(sx, sy, 0xffffff);
        if (sx > 0) fb_putpixel(sx-1, sy, 0x666688);
        if ((unsigned)(sx+1) < fb.width) fb_putpixel(sx+1, sy, 0x666688);
        if (sy > 0) fb_putpixel(sx, sy-1, 0x666688);
        if (sy < dh-1) fb_putpixel(sx, sy+1, 0x666688);
    }
}

static void draw_taskbar(void) {
    int ty = fb.height - TASKBAR_HEIGHT;

    // Taskbar background with subtle gradient
    fb_fill_gradient_v(0, ty, fb.width, TASKBAR_HEIGHT, 0x0e1628, COL_TASKBAR_BG);

    // Top border line
    fb_draw_hline(0, ty, fb.width, COL_TASKBAR_BORDER);
    fb_draw_hline(0, ty + 1, fb.width, 0x1a2a40);

    // ── Start button ──────────────────────────────────────────────────────────
    int bh = TASKBAR_HEIGHT - 8;
    int by = ty + 4;
    fb_fill_gradient_v(6, by, 56, bh, 0x5a8fbf, 0x3a6f9f);
    fb_draw_rect(6, by, 56, bh, 0x7aafdf);
    text_puts_centered(6, by + (bh - GLYPH_H)/2, 56, "Velox", 0xffffff, 0, 1);

    // ── Window buttons ────────────────────────────────────────────────────────
    int wx = 70;
    for (int i = 0; i < desktop.nwindows; i++) {
        window_t *w = &desktop.windows[i];
        if (!w->visible && !w->minimized) continue;

        int active = (i == desktop.active_win);
        uint32_t bg = active ? COL_TASKBAR_BTN_ACT : COL_TASKBAR_BTN;
        uint32_t border = active ? COL_TASKBAR_BORDER : 0x2a3a5a;

        fb_fill_rect(wx, by, 110, bh, bg);
        fb_draw_rect(wx, by, 110, bh, border);

        // Active indicator bar at bottom
        if (active)
            fb_fill_rect(wx+2, by + bh - 3, 106, 2, COL_TASKBAR_BORDER);

        // Dot indicator
        uint32_t dot = w->minimized ? 0x888888 : (active ? 0x80c0ff : 0x405070);
        fb_fill_rect(wx + 6, by + bh/2 - 2, 4, 4, dot);

        text_puts(wx + 14, by + (bh - GLYPH_H)/2, w->title,
                  active ? 0xf0f0f0 : 0x8899bb, 0, 1);

        wx += 118;
    }

    // ── System tray ──────────────────────────────────────────────────────────
    // Background
    int tray_w = 90;
    int tray_x = fb.width - tray_w - 4;
    fb_fill_rect(tray_x, by, tray_w, bh, COL_TRAY_BG);
    fb_draw_rect(tray_x, by, tray_w, bh, 0x2a3a5a);

    // Real time from RTC
    rtc_time_t t;
    rtc_read(&t);
    char timebuf[9];
    rtc_format_time(&t, timebuf);
    text_puts_centered(tray_x, by + (bh - GLYPH_H)/2, tray_w,
                       timebuf, COL_CLOCK, 0, 1);
}

static void draw_cursor_at(int mx, int my) {
    for (int r = 0; r < CURSOR_H; r++) {
        for (int c = 0; c < CURSOR_W; c++) {
            int px = mx + c, py = my + r;
            if (px < 0 || py < 0 ||
                px >= (int)fb.width || py >= (int)fb.height) continue;
            if      (cursor_bmp[r][c] == 1) fb_putpixel(px, py, 0x000000);
            else if (cursor_bmp[r][c] == 2) fb_putpixel(px, py, 0xffffff);
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void desktop_init(void) {
    desktop.nwindows    = 0;
    desktop.active_win  = -1;
    desktop.mx          = fb.width  / 2;
    desktop.my          = fb.height / 2;
    desktop.btn_left    = 0;
    desktop.dirty       = 1;
    desktop.needs_full_redraw = 1;
    desktop.cursor_saved = 0;
}

int desktop_add_window(int x, int y, int w, int h, const char *title) {
    if (desktop.nwindows >= MAX_WINDOWS) return -1;
    int idx = desktop.nwindows++;
    window_t *win = &desktop.windows[idx];
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    win->visible   = 1;
    win->minimized = 0;
    win->dragging  = 0;
    int i = 0;
    while (title[i] && i < TITLE_MAX - 1) { win->title[i] = title[i]; i++; }
    win->title[i] = 0;
    desktop.active_win = idx;
    desktop.needs_full_redraw = 1;
    return idx;
}

void desktop_redraw(void) {
    draw_wallpaper();
    for (int i = 0; i < desktop.nwindows; i++)
        window_draw(&desktop.windows[i], i == desktop.active_win);
    draw_taskbar();

    // Draw cursor into back buffer and save the region for fast restore
    // Clamp cursor save area
    int sx = desktop.mx, sy = desktop.my;
    if (sx + CURSOR_W > (int)fb.width)  sx = fb.width  - CURSOR_W;
    if (sy + CURSOR_H > (int)fb.height) sy = fb.height - CURSOR_H;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    fb_save_region(sx, sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
    desktop.cursor_saved = 1;
    desktop.cursor_sx = sx;
    desktop.cursor_sy = sy;

    draw_cursor_at(desktop.mx, desktop.my);
    fb_flip();

    desktop.dirty = 0;
    desktop.needs_full_redraw = 0;
}

void desktop_update_cursor(void) {
    // Restore old cursor area in back buffer and on screen
    if (desktop.cursor_saved) {
        fb_restore_region(desktop.cursor_sx, desktop.cursor_sy,
                          CURSOR_W, CURSOR_H, desktop.cursor_save);
        fb_flip_rect(desktop.cursor_sx, desktop.cursor_sy, CURSOR_W, CURSOR_H);
    }

    // Save new cursor area
    int sx = desktop.mx, sy = desktop.my;
    if (sx + CURSOR_W > (int)fb.width)  sx = fb.width  - CURSOR_W;
    if (sy + CURSOR_H > (int)fb.height) sy = fb.height - CURSOR_H;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    fb_save_region(sx, sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
    desktop.cursor_sx = sx;
    desktop.cursor_sy = sy;
    desktop.cursor_saved = 1;

    // Draw cursor and blit only cursor rect
    draw_cursor_at(desktop.mx, desktop.my);
    fb_flip_rect(sx, sy, CURSOR_W, CURSOR_H);
}

void desktop_mouse_move(int dx, int dy, int btn) {
    // Restore back buffer under old cursor before moving
    if (desktop.cursor_saved) {
        fb_restore_region(desktop.cursor_sx, desktop.cursor_sy,
                          CURSOR_W, CURSOR_H, desktop.cursor_save);
    }

    desktop.mx += dx;
    desktop.my += dy;
    if (desktop.mx < 0) desktop.mx = 0;
    if (desktop.my < 0) desktop.my = 0;
    if (desktop.mx >= (int)fb.width)  desktop.mx = fb.width  - 1;
    if (desktop.my >= (int)fb.height) desktop.my = fb.height - 1;

    int clicked = btn && !desktop.btn_left;
    desktop.btn_left = btn;

    if (clicked) {
        // Check window titlebar buttons first
        for (int i = desktop.nwindows - 1; i >= 0; i--) {
            window_t *w = &desktop.windows[i];
            if (!w->visible) continue;
            int hit = window_hit_button(w, desktop.mx, desktop.my);
            if (hit == BTN_CLOSE) {
                w->visible = 0;
                desktop.needs_full_redraw = 1;
                break;
            }
            if (hit == BTN_MIN) {
                w->minimized = !w->minimized;
                w->visible = !w->minimized;
                desktop.needs_full_redraw = 1;
                break;
            }
        }

        // Hit test for drag / focus
        for (int i = desktop.nwindows - 1; i >= 0; i--) {
            window_t *w = &desktop.windows[i];
            if (!w->visible) continue;
            if (desktop.mx >= w->x && desktop.mx < w->x + w->w &&
                desktop.my >= w->y && desktop.my < w->y + TITLEBAR_HEIGHT) {
                desktop.active_win = i;
                w->dragging = 1;
                w->drag_ox  = desktop.mx - w->x;
                w->drag_oy  = desktop.my - w->y;
                desktop.needs_full_redraw = 1;
                break;
            }
        }
    }

    if (!btn) {
        for (int i = 0; i < desktop.nwindows; i++) {
            if (desktop.windows[i].dragging) {
                desktop.windows[i].dragging = 0;
            }
        }
    }

    if (btn) {
        for (int i = 0; i < desktop.nwindows; i++) {
            window_t *w = &desktop.windows[i];
            if (w->dragging) {
                int new_x = desktop.mx - w->drag_ox;
                int new_y = desktop.my - w->drag_oy;
                if (new_y < 0) new_y = 0;
                if (new_y + w->h > (int)fb.height - TASKBAR_HEIGHT)
                    new_y = fb.height - TASKBAR_HEIGHT - w->h;
                // Only trigger redraw if window actually moved
                if (new_x != w->x || new_y != w->y) {
                    w->x = new_x;
                    w->y = new_y;
                    desktop.needs_full_redraw = 1;
                }
            }
        }
    }

    desktop.dirty = 1;
}