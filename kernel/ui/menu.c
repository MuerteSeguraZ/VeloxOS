#include "menu.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

context_menu_t ctx_menu;

// ── Colors ────────────────────────────────────────────────────────────────────
#define COL_MENU_BG       0x12121e
#define COL_MENU_BORDER   0x4a7fa5
#define COL_MENU_HOVER    0x1e3a5f
#define COL_MENU_TEXT     0xe0e0f0
#define COL_MENU_TEXT_DIM 0x607090
#define COL_MENU_SEP      0x2a3a5a
#define COL_MENU_SHADOW   0x000000

void menu_clear(void) {
    ctx_menu.nitems  = 0;
    ctx_menu.visible = 0;
    ctx_menu.hovered = -1;
}

void menu_add_item(const char *label, menu_action_t action) {
    if (ctx_menu.nitems >= MENU_MAX_ITEMS) return;
    menu_item_t *item = &ctx_menu.items[ctx_menu.nitems++];
    item->separator = 0;
    item->action    = action;
    int i = 0;
    while (label[i] && i < 31) { item->label[i] = label[i]; i++; }
    item->label[i] = 0;
}

void menu_add_separator(void) {
    if (ctx_menu.nitems >= MENU_MAX_ITEMS) return;
    menu_item_t *item = &ctx_menu.items[ctx_menu.nitems++];
    item->separator = 1;
    item->action    = 0;
    item->label[0]  = 0;
}

void menu_show(int x, int y) {
    // Calculate menu height
    int total_h = MENU_PADDING;
    for (int i = 0; i < ctx_menu.nitems; i++)
        total_h += ctx_menu.items[i].separator ? 7 : MENU_ITEM_H;
    total_h += MENU_PADDING;

    // Clamp to screen
    if (x + MENU_WIDTH > (int)fb.width)  x = fb.width  - MENU_WIDTH - 2;
    if (y + total_h   > (int)fb.height)  y = fb.height - total_h    - 2;

    ctx_menu.x       = x;
    ctx_menu.y       = y;
    ctx_menu.visible = 1;
    ctx_menu.hovered = -1;
}

void menu_hide(void) {
    ctx_menu.visible = 0;
    ctx_menu.hovered = -1;
}

// Calculate the y position of item i inside the menu
static int item_y(int i) {
    int y = ctx_menu.y + MENU_PADDING;
    for (int j = 0; j < i; j++)
        y += ctx_menu.items[j].separator ? 7 : MENU_ITEM_H;
    return y;
}

static int menu_height(void) {
    int h = MENU_PADDING * 2;
    for (int i = 0; i < ctx_menu.nitems; i++)
        h += ctx_menu.items[i].separator ? 7 : MENU_ITEM_H;
    return h;
}

void menu_draw(void) {
    if (!ctx_menu.visible) return;

    int x = ctx_menu.x;
    int y = ctx_menu.y;
    int w = MENU_WIDTH;
    int h = menu_height();

    // Shadow
    fb_fill_rect(x + 4, y + 4, w, h, COL_MENU_SHADOW);

    // Background
    fb_fill_rect(x, y, w, h, COL_MENU_BG);

    // Border
    fb_draw_rect(x, y, w, h, COL_MENU_BORDER);

    // Inner highlight line at top
    fb_draw_hline(x + 1, y + 1, w - 2, 0x2a4a6a);

    // Items
    for (int i = 0; i < ctx_menu.nitems; i++) {
        menu_item_t *item = &ctx_menu.items[i];
        int iy = item_y(i);

        if (item->separator) {
            // Divider line centered vertically in 7px slot
            fb_draw_hline(x + 8, iy + 3, w - 16, COL_MENU_SEP);
            continue;
        }

        // Hover highlight
        if (i == ctx_menu.hovered) {
            fb_fill_rect(x + 1, iy, w - 2, MENU_ITEM_H, COL_MENU_HOVER);
            fb_draw_hline(x + 1, iy,                  w - 2, 0x2a5a8a);
            fb_draw_hline(x + 1, iy + MENU_ITEM_H - 1, w - 2, 0x1a3a5a);
        }

        // Icon placeholder dot
        fb_fill_rect(x + 8, iy + MENU_ITEM_H/2 - 2, 4, 4,
                     i == ctx_menu.hovered ? 0x80c0ff : 0x405070);

        // Label
        text_puts(x + 18, iy + (MENU_ITEM_H - GLYPH_H) / 2,
                  item->label,
                  i == ctx_menu.hovered ? COL_MENU_TEXT : COL_MENU_TEXT_DIM,
                  0, 1);
    }
}

int menu_handle_click(int mx, int my) {
    if (!ctx_menu.visible) return 0;

    int x = ctx_menu.x;
    int y = ctx_menu.y;
    int w = MENU_WIDTH;
    int h = menu_height();

    // Click outside — close menu
    if (mx < x || mx >= x + w || my < y || my >= y + h) {
        menu_hide();
        return 1;   // consumed — don't pass click to desktop
    }

    // Find which item was clicked
    for (int i = 0; i < ctx_menu.nitems; i++) {
        menu_item_t *item = &ctx_menu.items[i];
        if (item->separator) continue;
        int iy = item_y(i);
        if (my >= iy && my < iy + MENU_ITEM_H) {
            menu_hide();
            if (item->action) item->action();
            return 1;
        }
    }

    return 1;   // click inside menu but not on item — still consumed
}

void menu_handle_hover(int mx, int my) {
    if (!ctx_menu.visible) return;

    int x = ctx_menu.x;
    int y = ctx_menu.y;
    int w = MENU_WIDTH;
    int h = menu_height();

    if (mx < x || mx >= x + w || my < y || my >= y + h) {
        ctx_menu.hovered = -1;
        return;
    }

    for (int i = 0; i < ctx_menu.nitems; i++) {
        if (ctx_menu.items[i].separator) continue;
        int iy = item_y(i);
        if (my >= iy && my < iy + MENU_ITEM_H) {
            ctx_menu.hovered = i;
            return;
        }
    }
    ctx_menu.hovered = -1;
}