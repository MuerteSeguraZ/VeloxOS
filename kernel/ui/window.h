#pragma once
#include "../stdint.h"

#define MAX_WINDOWS     8
#define TITLE_MAX       48
#define TITLEBAR_HEIGHT 22
#define WINDOW_BORDER   2
#define CURSOR_W        12
#define CURSOR_H        12

#define WIN_CONTENT_MAX 1024   // max bytes of text content a window can show

#define BTN_CLOSE 1
#define BTN_MIN   2
#define BTN_MAX   3

typedef struct {
    int  x, y, w, h;
    char title[TITLE_MAX];
    int  visible;
    int  minimized;
    int  dragging;
    int  drag_ox, drag_oy;

    // Optional text content (e.g. file contents)
    char content[WIN_CONTENT_MAX];
    int  has_content;
} window_t;

void window_draw(window_t *win, int active);
int  window_hit_button(window_t *win, int mx, int my);
void window_set_content(window_t *win, const char *text, uint32_t len);