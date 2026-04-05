#pragma once
#include "../stdint.h"
#include "window.h"

#define TASKBAR_HEIGHT  32

// ── Colors ────────────────────────────────────────────────────────────────────
#define COL_DESKTOP_TOP     0x0d0d1a
#define COL_DESKTOP_BOT     0x1a1a35
#define COL_TASKBAR_BG      0x0a0a18
#define COL_TASKBAR_BORDER  0x4a7fa5
#define COL_TASKBAR_BTN     0x1a2a4a
#define COL_TASKBAR_BTN_ACT 0x2a4a7a
#define COL_START_BG        0x4a7fa5
#define COL_START_HOV       0x6a9fc5
#define COL_CLOCK           0xc0d8f0
#define COL_TRAY_BG         0x111122

typedef struct {
    window_t windows[MAX_WINDOWS];
    int      nwindows;
    int      active_win;

    // Mouse
    int      mx, my;
    int      btn_left;

    // Cursor save buffer (CURSOR_W * CURSOR_H pixels)
    uint32_t cursor_save[CURSOR_W * CURSOR_H];
    int      cursor_saved;
    int      cursor_sx, cursor_sy;   // where we saved from

    // Dirty flag
    int      dirty;
    int      needs_full_redraw;
} desktop_t;

extern desktop_t desktop;

void desktop_init(void);
int  desktop_add_window(int x, int y, int w, int h, const char *title);

// Full redraw — wallpaper + windows + taskbar → back buffer, then flip
void desktop_redraw(void);

// Fast update — restore cursor area, move cursor, blit only cursor rect
void desktop_update_cursor(void);

// Input
void desktop_mouse_move(int dx, int dy, int btn);
