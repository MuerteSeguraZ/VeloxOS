#pragma once
#include "../stdint.h"

#define MAX_WINDOWS     8
#define TITLE_MAX       48
#define TITLEBAR_HEIGHT 22
#define WINDOW_BORDER   2
#define CURSOR_W        12
#define CURSOR_H        12

// ── Window close/min/max button hit areas ────────────────────────────────────
#define BTN_CLOSE 1
#define BTN_MIN   2
#define BTN_MAX   3

typedef struct {
    int  x, y, w, h;
    char title[TITLE_MAX];
    int  visible;
    int  minimized;

    // Drag state
    int  dragging;
    int  drag_ox, drag_oy;
} window_t;

// Draw a single window (active = 1 if focused)
void window_draw(window_t *win, int active);

// Hit test a click on a window's titlebar buttons — returns BTN_* or 0
int window_hit_button(window_t *win, int mx, int my);
