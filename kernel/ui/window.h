#pragma once
#include "../stdint.h"

#define MAX_WINDOWS      8
#define TITLE_MAX        48
#define TITLEBAR_HEIGHT  22
#define WINDOW_BORDER    2
#define CURSOR_W         12
#define CURSOR_H         12

#define WIN_CONTENT_MAX  2048   // max bytes of content / edit buffer

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

    // Display content (read from disk)
    char content[WIN_CONTENT_MAX];
    int  has_content;

    // Editing
    int  editable;                  // 1 = text file, accepts keyboard input
    char edit_buf[WIN_CONTENT_MAX]; // working copy user edits
    char orig_buf[WIN_CONTENT_MAX]; // untouched copy for revert
    int  edit_len;                  // current length of edit_buf
    int  edit_dirty;                // 1 = unsaved changes
    int  fs_idx;                    // which vfs entry backs this window (-1 = none)

    // Scroll
    int  scroll_row;                // first visible row
} window_t;

void window_draw(window_t *win, int active);
int  window_hit_button(window_t *win, int mx, int my);
void window_set_content(window_t *win, const char *text, uint32_t len);
void window_set_editable(window_t *win, const char *text, uint32_t len, int fs_idx);

// Feed a scancode to an editable window — returns 1 if consumed
int  window_handle_key(window_t *win, uint8_t scancode);