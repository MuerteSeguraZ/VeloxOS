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

#define COL_WIN_TITLEBAR_ACT   0x1e3a5f
#define COL_WIN_TITLEBAR_INACT 0x1a1a2e
#define COL_WIN_TITLEBAR_GRAD  0x0d2137
#define COL_WIN_BODY           0x0e0e1a
#define COL_WIN_BODY2          0x181828
#define COL_WIN_BORDER_ACT     0x4a7fa5
#define COL_WIN_BORDER_INACT   0x2a2a4a
#define COL_TEXT_WHITE         0xf0f0f0
#define COL_TEXT_DIM           0x7080a0
#define COL_TEXT_CONTENT       0xc8d8e8
#define COL_TEXT_CURSOR        0x80c0ff
#define COL_BTN_CLOSE          0xe05050
#define COL_BTN_MIN            0xe0a030
#define COL_BTN_MAX            0x40b060
#define COL_DIRTY_DOT          0xe0a030

struct explorer_t;
struct shell_t;

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
    int  folder_idx;
    int  scroll_row;
    struct explorer_t *explorer;
    struct shell_t *shell;
} window_t;

void window_draw(window_t *win, int active);
int  window_hit_button(window_t *win, int mx, int my);
void window_set_content(window_t *win, const char *text, uint32_t len);
void window_set_editable(window_t *win, const char *text, uint32_t len, int fs_idx);
int  window_handle_key(window_t *win, const key_event_t *evt);
void window_set_folder(window_t *win, int folder_idx);