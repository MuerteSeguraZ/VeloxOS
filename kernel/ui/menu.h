#pragma once
#include "../stdint.h"

#define MENU_MAX_ITEMS  8
#define MENU_ITEM_H     20
#define MENU_WIDTH      160
#define MENU_PADDING    6

#define COL_MENU_BG       0x12121e
#define COL_MENU_BORDER   0x4a7fa5
#define COL_MENU_HOVER    0x1e3a5f
#define COL_MENU_TEXT     0xe0e0f0
#define COL_MENU_TEXT_DIM 0x607090
#define COL_MENU_SEP      0x2a3a5a
#define COL_MENU_SHADOW   0x000000

typedef void (*menu_action_t)(void);

typedef struct {
    char          label[32];
    menu_action_t action;
    int           separator;
} menu_item_t;

typedef struct {
    int         visible;
    int         x, y;
    int         hovered;
    menu_item_t items[MENU_MAX_ITEMS];
    int         nitems;
} context_menu_t;

extern context_menu_t ctx_menu;

void menu_clear(void);
void menu_add_item(const char *label, menu_action_t action);
void menu_add_separator(void);
void menu_show(int x, int y);
void menu_hide(void);
void menu_draw(void);
int  menu_handle_click(int mx, int my);
void menu_handle_hover(int mx, int my);