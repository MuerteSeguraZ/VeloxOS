#pragma once
#include "../stdint.h"
#include "window.h"
#include "menu.h"
#include "input.h"
#include "../drivers/keyboard/keyboard.h"

#define TASKBAR_HEIGHT  32

#define COL_DESKTOP_TOP     0x0d0d1a
#define COL_DESKTOP_BOT     0x1a1a35
#define COL_TASKBAR_BG      0x0a0a18
#define COL_TASKBAR_BORDER  0x4a7fa5
#define COL_TASKBAR_BTN     0x1a2a4a
#define COL_TASKBAR_BTN_ACT 0x2a4a7a
#define COL_CLOCK           0xc0d8f0
#define COL_TRAY_BG         0x111122

// Window node for linked list
typedef struct window_node {
    window_t *win;
    struct window_node *next;
} window_node_t;

typedef struct {
    window_node_t *windows;     // Head of linked list
    int           nwindows;     // Count of windows (for quick access)
    window_node_t *active_win;  // Pointer to active window node (instead of index)
    int           mx, my;
    int           btn_left, btn_right;
    uint32_t      cursor_save[CURSOR_W * CURSOR_H];
    int           cursor_saved;
    int           cursor_sx, cursor_sy;
    int           dirty;
    int           needs_full_redraw;
    int           selected_icon;
} desktop_t;

extern desktop_t desktop;

void desktop_init(void);
window_node_t *desktop_add_window(int x, int y, int w, int h, const char *title);
void desktop_redraw(void);
void desktop_update_cursor(void);
void desktop_mouse_move(int dx, int dy, int btn_left, int btn_right);
void desktop_handle_key(const key_event_t *evt);