#pragma once
#include "../stdint.h"
#include "../drivers/keyboard/keyboard.h"

#define MAX_WINDOWS      8
#define TITLE_MAX        48
#define TITLEBAR_HEIGHT  22
#define WINDOW_BORDER    2
#define CURSOR_W         12
#define CURSOR_H         12
#define WIN_CONTENT_MAX  2048

#define BTN_CLOSE  1
#define BTN_MIN    2
#define BTN_MAX    3

typedef struct {
    int  x, y, w, h;
    char title[TITLE_MAX];
    int  visible;
    int  minimized;
    int  dragging;
    int  drag_ox, drag_oy;
    char content[WIN_CONTENT_MAX];
    int  has_content;
    int  editable;
    char edit_buf[WIN_CONTENT_MAX];
    char orig_buf[WIN_CONTENT_MAX];
    int  edit_len;
    int  edit_dirty;
    int  fs_idx;
    int  scroll_row;
} window_t;

void window_draw(window_t *win, int active);
int  window_hit_button(window_t *win, int mx, int my);
void window_set_content(window_t *win, const char *text, uint32_t len);
void window_set_editable(window_t *win, const char *text, uint32_t len, int fs_idx);
int  window_handle_key(window_t *win, const key_event_t *evt);