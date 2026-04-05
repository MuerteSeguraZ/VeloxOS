#pragma once
#include "../stdint.h"

#define MENU_MAX_ITEMS  8
#define MENU_ITEM_H     20
#define MENU_WIDTH      160
#define MENU_PADDING    6

typedef void (*menu_action_t)(void);

typedef struct {
    char          label[32];
    menu_action_t action;
    int           separator;    // 1 = draw a divider line instead of a label
} menu_item_t;

typedef struct {
    int         visible;
    int         x, y;
    int         hovered;        // index of hovered item, -1 = none
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

// Returns 1 if menu consumed the click, 0 if click was outside
int  menu_handle_click(int mx, int my);
void menu_handle_hover(int mx, int my);