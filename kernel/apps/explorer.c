#include "explorer.h"
#include "../ui/desktop.h"
#include "../ui/window.h"
#include "../ui/input.h"
#include "../ui/menu.h"
#include "../fs/fs.h"
#include "../mm/alloc.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

static const char *const sidebar_labels[EXPLO_SIDEBAR_ITEMS] = {
    "Desktop", "Documents", "Trash"
};

static const char *const sidebar_targets[EXPLO_SIDEBAR_ITEMS] = {
    NULL, "Documents", "Trash"
};

#define TBTN_BACK    0
#define TBTN_UP      1
#define TBTN_REFRESH 2
#define TBTN_NEWFILE 3
#define TBTN_NEWFLD  4
#define TBTN_COUNT   5

static const char *const tbtn_labels[TBTN_COUNT] = {
    "Back", "Up", "Refresh", "+File", "+Folder"
};
static const int tbtn_w[TBTN_COUNT] = { 50, 40, 62, 50, 70 };

static int ex_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}
static void ex_strcpy(char *d, const char *s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}
static void ex_itoa(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + n % 10; n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = 0;
}
static void ex_memset(void *p, uint8_t v, int n) {
    uint8_t *b = (uint8_t *)p; while (n--) *b++ = v;
}

static int toolbar_x(window_t *w) { return w->x; }
static int toolbar_y(window_t *w) { return w->y + TITLEBAR_HEIGHT; }
static int sidebar_x(window_t *w) { return w->x; }
static int sidebar_y(window_t *w) { return w->y + TITLEBAR_HEIGHT + EXPLO_TOOLBAR_H; }
static int sidebar_h(window_t *w) { return w->h - TITLEBAR_HEIGHT - EXPLO_TOOLBAR_H - EXPLO_STATUSBAR_H; }
static int content_x(window_t *w) { return w->x + EXPLO_SIDEBAR_W; }
static int content_y(window_t *w) { return w->y + TITLEBAR_HEIGHT + EXPLO_TOOLBAR_H; }
static int content_w(window_t *w) { return w->w - EXPLO_SIDEBAR_W; }
static int content_h(window_t *w) { return w->h - TITLEBAR_HEIGHT - EXPLO_TOOLBAR_H - EXPLO_STATUSBAR_H; }
static int statusbar_y(window_t *w) { return w->y + w->h - EXPLO_STATUSBAR_H; }

static int max_visible_rows(window_t *w) {
    int r = content_h(w) / EXPLO_ITEM_H;
    return r < 1 ? 1 : r;
}

static int tbtn_px(window_t *w, int btn) {
    int x = toolbar_x(w) + 4;
    for (int i = 0; i < btn; i++) {
        x += tbtn_w[i] + 3;
        if (i == 1) x += 8;
    }
    return x;
}
static int tbtn_hit(window_t *w, int btn, int mx, int my) {
    int ty  = toolbar_y(w);
    int bx  = tbtn_px(w, btn);
    int bh  = EXPLO_TOOLBAR_H - 6;
    int by  = ty + 3;
    return mx >= bx && mx < bx + tbtn_w[btn] && my >= by && my < by + bh;
}

static explorer_t *g_menu_ex = NULL;
static int g_menu_item_idx = -1;

void explorer_refresh(explorer_t *ex) {
    ex->item_count = 0;
    ex->selected   = -1;
    ex->scroll     = 0;
    ex->hover      = -1;
    if (!vfs.mounted) return;

    int parent = VFS_ROOT_PARENT;
    if (ex->nav_depth > 0)
        parent = ex->nav_stack[ex->nav_depth - 1].dir_idx;

    explo_item_t dirs[VFS_MAX_FILES];
    explo_item_t files[VFS_MAX_FILES];
    int di = 0, fi = 0;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != parent) continue;
        vfs_entry_t *e = &vfs.entries[i];
        explo_item_t it;
        it.fs_idx = i;
        ex_strcpy(it.name, e->name, VFS_NAME_MAX);
        it.is_dir = e->is_dir;
        it.size_bytes = e->size_bytes;
        if (e->is_dir) dirs[di++] = it;
        else           files[fi++] = it;
    }
    for (int i = 0; i < di; i++) ex->items[ex->item_count++] = dirs[i];
    for (int i = 0; i < fi; i++) ex->items[ex->item_count++] = files[i];
    ex->needs_refresh = 0;
}

void explorer_navigate(explorer_t *ex, int dir_idx, const char *name) {
    if (ex->nav_depth >= EXPLO_MAX_PATH_DEPTH) return;
    explo_nav_entry_t *e = &ex->nav_stack[ex->nav_depth++];
    e->dir_idx = dir_idx;
    ex_strcpy(e->name, name, VFS_NAME_MAX);
    explorer_refresh(ex);
}

void explorer_up(explorer_t *ex) {
    if (ex->nav_depth <= 0) return;
    ex->nav_depth--;
    explorer_refresh(ex);
}

void explorer_open_selected(explorer_t *ex) {
    if (ex->selected < 0 || ex->selected >= ex->item_count) return;
    explo_item_t *it = &ex->items[ex->selected];
    if (it->is_dir) {
        explorer_navigate(ex, it->fs_idx, it->name);
    } else {
        window_node_t *node = desktop_add_window(120, 80, 400, 280, it->name);
        if (node) {
            vfs_entry_t *e = &vfs.entries[it->fs_idx];
            if (e->size_bytes > 0) {
                void *buf = mm_alloc(e->size_bytes + 1);
                if (buf) {
                    uint32_t n = vfs_read(it->fs_idx, buf);
                    ((char *)buf)[n] = 0;
                    window_set_editable(node->win, (char *)buf, n, it->fs_idx);
                    mm_free(buf);
                }
            } else {
                window_set_editable(node->win, "", 0, it->fs_idx);
            }
        }
        desktop.needs_full_redraw = 1;
    }
}

static void on_rename_confirm(const char *text, void *ud) {
    (void)ud;
    explorer_t *ex = g_menu_ex;
    if (!ex || !text || !text[0] || g_menu_item_idx < 0) return;
    if (g_menu_item_idx >= ex->item_count) return;
    int fs_idx = ex->items[g_menu_item_idx].fs_idx;
    if (!vfs.entries[fs_idx].used) return;
    int parent = VFS_ROOT_PARENT;
    if (ex->nav_depth > 0) parent = ex->nav_stack[ex->nav_depth - 1].dir_idx;
    if (vfs_find_in(text, parent) >= 0) return;
    int i = 0;
    while (text[i] && i < VFS_NAME_MAX - 1) { vfs.entries[fs_idx].name[i] = text[i]; i++; }
    vfs.entries[fs_idx].name[i] = 0;
    vfs_flush();
    explorer_refresh(ex);
    g_menu_item_idx = -1;
    desktop.needs_full_redraw = 1;
}

static void on_rename_cancel(void *ud) {
    (void)ud;
    g_menu_item_idx = -1;
    desktop.needs_full_redraw = 1;
}

static void on_delete_yes(const char *text, void *ud) {
    (void)text; (void)ud;
    if (g_menu_item_idx < 0) return;
    explorer_t *ex = g_menu_ex;
    if (!ex) return;
    int fs_idx = ex->items[g_menu_item_idx].fs_idx;
    vfs_delete(fs_idx);
    explorer_refresh(ex);
    g_menu_item_idx = -1;
    desktop.needs_full_redraw = 1;
}

static void on_delete_no(void *ud) { (void)ud; desktop.needs_full_redraw = 1; }
static void on_delete_cancel(void *ud) { (void)ud; desktop.needs_full_redraw = 1; }

static void action_rename(void) {
    if (g_menu_item_idx < 0 || !g_menu_ex) return;
    explorer_t *ex = g_menu_ex;
    if (g_menu_item_idx >= ex->item_count) return;
    input_show("Rename", ex->items[g_menu_item_idx].name,
               on_rename_confirm, on_rename_cancel, 0);
    desktop.needs_full_redraw = 1;
}

static void action_delete(void) {
    if (g_menu_item_idx < 0 || !g_menu_ex) return;
    explorer_t *ex = g_menu_ex;
    if (g_menu_item_idx >= ex->item_count) return;
    explo_item_t *it = &ex->items[g_menu_item_idx];
    char msg[64];
    int i = 0;
    const char *p = "Delete '"; while (*p && i < 60) msg[i++] = *p++;
    while (it->name[i-8] && i < 60) { msg[i++] = it->name[i-8]; }
    msg[i++] = '\''; msg[i++] = '?'; msg[i] = 0;
    input_show_ync("Confirm Delete", msg,
                   on_delete_yes, on_delete_no, on_delete_cancel, 0);
    desktop.needs_full_redraw = 1;
}

void explorer_draw(explorer_t *ex, window_t *win, int active) {
    if (!win->visible) return;
    (void)active;
    int wx = win->x, ww = win->w;

    {
        int ty = toolbar_y(win);
        fb_fill_rect(wx, ty, ww, EXPLO_TOOLBAR_H, COL_EXPLO_TOOLBAR_BG);
        fb_draw_hline(wx, ty + EXPLO_TOOLBAR_H - 1, ww, COL_EXPLO_TOOLBAR_BD);

        for (int b = 0; b < TBTN_COUNT; b++) {
            int bx = tbtn_px(win, b);
            int bw = tbtn_w[b];
            int by = ty + 3;
            int bh = EXPLO_TOOLBAR_H - 6;

            int greyed =
                (b == TBTN_BACK    && ex->nav_depth == 0) ||
                (b == TBTN_UP      && ex->nav_depth == 0);

            int hov = 0;
            if (!greyed) {
                hov = (b == TBTN_BACK    && ex->btn_back_hover)    ||
                      (b == TBTN_UP      && ex->btn_up_hover)      ||
                      (b == TBTN_REFRESH && ex->btn_refresh_hover) ||
                      (b == TBTN_NEWFILE && ex->btn_newfile_hover) ||
                      (b == TBTN_NEWFLD  && ex->btn_newfolder_hover);
            }

            uint32_t bg = hov ? COL_EXPLO_BTN_HOV : COL_EXPLO_BTN_BG;
            fb_fill_rect(bx, by, bw, bh, bg);
            fb_draw_rect(bx, by, bw, bh, COL_EXPLO_BTN_BD);
            uint32_t fg = greyed ? COL_EXPLO_TEXT_DIM : COL_EXPLO_TEXT;
            text_puts_centered(bx, by + (bh - 8) / 2, bw, tbtn_labels[b], fg, 0, 1);
        }

        int bc_x = tbtn_px(win, TBTN_COUNT - 1) + tbtn_w[TBTN_COUNT - 1] + 10;
        int bc_y = ty + (EXPLO_TOOLBAR_H - 8) / 2;
        int bc_max = wx + ww - bc_x - 4;
        if (bc_max > 20) {
            char path[128];
            int pi = 0;
            path[pi++] = '/';
            for (int d = 0; d < ex->nav_depth && pi < 120; d++) {
                const char *n = ex->nav_stack[d].name;
                while (*n && pi < 120) path[pi++] = *n++;
                if (d < ex->nav_depth - 1 && pi < 120) path[pi++] = '/';
            }
            path[pi] = 0;
            int pw = ex_strlen(path) * (8 + 1);
            if (pw > bc_max) {
                int skip = (pw - bc_max) / (8 + 1) + 3;
                text_puts(bc_x,      bc_y, "...", COL_EXPLO_BREADCRUMB, 0, 1);
                text_puts(bc_x + 22, bc_y, path + skip, COL_EXPLO_BREADCRUMB, 0, 1);
            } else {
                text_puts(bc_x, bc_y, path, COL_EXPLO_BREADCRUMB, 0, 1);
            }
        }
    }

    {
        int sx = sidebar_x(win);
        int sy = sidebar_y(win);
        int sh = sidebar_h(win);
        fb_fill_rect(sx, sy, EXPLO_SIDEBAR_W, sh, COL_EXPLO_SIDEBAR_BG);
        fb_draw_vline(sx + EXPLO_SIDEBAR_W - 1, sy, sh, COL_EXPLO_SIDEBAR_BD);

        text_puts(sx + 8, sy + 6, "PLACES", COL_EXPLO_TEXT_DIM, 0, 1);
        fb_draw_hline(sx + 4, sy + 18, EXPLO_SIDEBAR_W - 8, 0x1e2e44);

        for (int i = 0; i < EXPLO_SIDEBAR_ITEMS; i++) {
            int iy = sy + 24 + i * EXPLO_ITEM_H;
            int is_root = (sidebar_targets[i] == NULL);
            int target_idx = is_root ? -1 : vfs_find(sidebar_targets[i]);
            int available = is_root ||
                (target_idx >= 0 && vfs.entries[target_idx].is_dir);

            int sel = 0;
            if (is_root && ex->nav_depth == 0) {
                sel = 1;
            } else if (!is_root && target_idx >= 0 && ex->nav_depth > 0 &&
                       ex->nav_stack[ex->nav_depth - 1].dir_idx == target_idx) {
                sel = 1;
            }

            if (sel)
                fb_fill_rect(sx + 2, iy, EXPLO_SIDEBAR_W - 4, EXPLO_ITEM_H, COL_EXPLO_SEL_BG);
            else if (ex->sidebar_hover == i && available)
                fb_fill_rect(sx + 2, iy, EXPLO_SIDEBAR_W - 4, EXPLO_ITEM_H, COL_EXPLO_HOVER_BG);

            if (available) {
                fb_fill_rect(sx + 8, iy + 4, 14, 9, 0x3a6a9a);
                fb_fill_rect(sx + 8, iy + 3,  5, 4, 0x4a7fa5);
                fb_draw_rect(sx + 8, iy + 4, 14, 9, 0x5a9fbf);
            }
            uint32_t fc = available ? (sel ? 0xd0e8ff : COL_EXPLO_TEXT) : COL_EXPLO_TEXT_DIM;
            text_puts(sx + 26, iy + 5, sidebar_labels[i], fc, 0, 1);
        }
    }

    {
        int cx  = content_x(win);
        int cy  = content_y(win);
        int cw  = content_w(win);
        int ch  = content_h(win);
        int mvr = max_visible_rows(win);

        fb_fill_rect(cx, cy, cw, ch, COL_EXPLO_CONTENT_BG);

        if (!vfs.mounted) {
            text_puts(cx + 8, cy + 8, "No filesystem mounted.", COL_EXPLO_TEXT_DIM, 0, 1);
        } else if (ex->item_count == 0) {
            text_puts(cx + 8, cy + 8,  "(empty)",                  COL_EXPLO_TEXT_DIM, 0, 1);
            text_puts(cx + 8, cy + 24, "Use toolbar to add files.", 0x2a3a4a,           0, 1);
        } else {
            int visible = ex->item_count - ex->scroll;
            if (visible > mvr) visible = mvr;

            for (int r = 0; r < visible; r++) {
                int ii = r + ex->scroll;
                explo_item_t *it = &ex->items[ii];
                int iy = cy + r * EXPLO_ITEM_H;

                if (ii == ex->selected) {
                    fb_fill_rect(cx, iy, cw, EXPLO_ITEM_H, COL_EXPLO_SEL_BG);
                    fb_draw_hline(cx, iy,                    cw, COL_EXPLO_SEL_BD);
                    fb_draw_hline(cx, iy + EXPLO_ITEM_H - 1, cw, COL_EXPLO_SEL_BD);
                } else if (ii == ex->hover) {
                    fb_fill_rect(cx, iy, cw, EXPLO_ITEM_H, COL_EXPLO_HOVER_BG);
                } else {
                    uint32_t row_bg = (ii % 2) ? COL_EXPLO_CONTENT_BG : COL_EXPLO_CONTENT_BG2;
                    fb_fill_rect(cx, iy, cw, EXPLO_ITEM_H, row_bg);
                }

                if (it->is_dir) {
                    fb_fill_rect(cx + 5, iy + 4, 14, 9, 0x3a6a9a);
                    fb_fill_rect(cx + 5, iy + 3,  5, 4, 0x4a7fa5);
                    fb_draw_rect(cx + 5, iy + 4, 14, 9, 0x5a9fbf);
                } else {
                    fb_fill_rect(cx + 5,  iy + 3, 12, 11, COL_EXPLO_ICON_FILE);
                    fb_fill_rect(cx + 14, iy + 3,  3,  3, 0x1a3a2a);
                    fb_fill_rect(cx + 14, iy + 6,  3,  8, COL_EXPLO_ICON_FILE);
                    fb_draw_rect(cx + 5,  iy + 3, 12, 11, 0x4a9a6a);
                    fb_draw_hline(cx + 7, iy + 7,  8, 0x4a9a6a);
                    fb_draw_hline(cx + 7, iy + 10, 8, 0x3a7a5a);
                }

                uint32_t tc = it->is_dir ? COL_EXPLO_TEXT_DIR : COL_EXPLO_TEXT_FILE;
                if (ii == ex->selected) tc = 0xf0f8ff;
                text_puts(cx + 24, iy + 5, it->name, tc, 0, 1);

                if (!it->is_dir && it->size_bytes > 0) {
                    char sz[16];
                    if (it->size_bytes < 1024) {
                        ex_itoa(it->size_bytes, sz);
                        int si = ex_strlen(sz);
                        sz[si++] = ' '; sz[si++] = 'B'; sz[si] = 0;
                    } else {
                        ex_itoa(it->size_bytes / 1024, sz);
                        int si = ex_strlen(sz);
                        sz[si++] = ' '; sz[si++] = 'K'; sz[si++] = 'B'; sz[si] = 0;
                    }
                    int sw = ex_strlen(sz) * (8 + 1);
                    text_puts(cx + cw - sw - 8, iy + 5, sz, COL_EXPLO_TEXT_DIM, 0, 1);
                } else if (it->is_dir) {
                    int cc = 0;
                    for (int j = 0; j < VFS_MAX_FILES; j++)
                        if (vfs.entries[j].used && (int)vfs.entries[j].parent_idx == it->fs_idx) cc++;
                    if (cc > 0) {
                        char info[16]; int ci = 0;
                        char num[8]; ex_itoa(cc, num);
                        const char *p = num; while (*p) info[ci++] = *p++;
                        const char *s = " item"; while (*s) info[ci++] = *s++;
                        if (cc != 1) info[ci++] = 's';
                        info[ci] = 0;
                        int iw = ex_strlen(info) * (8 + 1);
                        text_puts(cx + cw - iw - 8, iy + 5, info, COL_EXPLO_TEXT_DIM, 0, 1);
                    }
                }
            }

            if (ex->item_count > mvr) {
                int sb_x = cx + cw - 5;
                fb_fill_rect(sb_x, cy, 4, ch, 0x0a0a14);
                int th = mvr * ch / ex->item_count;
                if (th < 6) th = 6;
                int ty2 = ch * ex->scroll / ex->item_count;
                fb_fill_rect(sb_x + 1, cy + ty2, 2, th, 0x4a7fa5);
            }
        }
    }

    {
        int sy = statusbar_y(win);
        fb_fill_rect(wx, sy, ww, EXPLO_STATUSBAR_H, COL_EXPLO_STATUS_BG);
        fb_draw_hline(wx, sy, ww, COL_EXPLO_SIDEBAR_BD);

        char status[96]; int si = 0;
        char cnt[8]; ex_itoa(ex->item_count, cnt);
        const char *p = cnt; while (*p) status[si++] = *p++;
        const char *m = " item"; while (*m) status[si++] = *m++;
        if (ex->item_count != 1) status[si++] = 's';

        if (ex->selected >= 0 && ex->selected < ex->item_count) {
            explo_item_t *it = &ex->items[ex->selected];
            status[si++] = ','; status[si++] = ' ';
            p = it->name; while (*p && si < 80) status[si++] = *p++;
            if (!it->is_dir && it->size_bytes > 0) {
                status[si++] = ' '; status[si++] = '(';
                char sz[8]; ex_itoa(it->size_bytes, sz);
                p = sz; while (*p) status[si++] = *p++;
                status[si++] = 'B'; status[si++] = ')';
            }
        }
        status[si] = 0;
        text_puts(wx + 6, sy + (EXPLO_STATUSBAR_H - 8) / 2, status, COL_EXPLO_STATUS_FG, 0, 1);
    }
}

int explorer_handle_key(explorer_t *ex, window_t *win, const key_event_t *evt) {
    if (!evt->pressed) return 0;
    int mvr = max_visible_rows(win);

    switch (evt->scancode) {
        case KEY_UP:
            if (ex->selected > 0) ex->selected--;
            else if (ex->item_count > 0) ex->selected = 0;
            if (ex->selected < ex->scroll) ex->scroll = ex->selected;
            desktop.needs_full_redraw = 1;
            return 1;

        case KEY_DOWN:
            if (ex->selected < ex->item_count - 1) ex->selected++;
            else if (ex->item_count > 0) ex->selected = ex->item_count - 1;
            if (ex->selected >= ex->scroll + mvr) ex->scroll = ex->selected - mvr + 1;
            desktop.needs_full_redraw = 1;
            return 1;

        case KEY_ENTER:
            if (ex->selected >= 0) explorer_open_selected(ex);
            desktop.needs_full_redraw = 1;
            return 1;

        case KEY_BACKSPACE:
            explorer_up(ex);
            desktop.needs_full_redraw = 1;
            return 1;

        case KEY_DELETE:
        if (ex->selected >= 0 && ex->selected < ex->item_count) {
            explo_item_t *it = &ex->items[ex->selected];
            g_menu_ex = ex;
            g_menu_item_idx = ex->selected;
            char msg[64];
            int i = 0;
            const char *p = "Delete '"; 
            while (*p && i < 60) msg[i++] = *p++;
            while (it->name[i-8] && i < 60) msg[i++] = it->name[i-8];
            msg[i++] = '\''; 
            msg[i++] = '?'; 
            msg[i] = 0;
            input_show_ync("Confirm Delete", msg,
                           on_delete_yes, on_delete_no, on_delete_cancel, 0);
            desktop.needs_full_redraw = 1;
        }
        return 1;

        case KEY_F2:
            if (ex->selected >= 0 && ex->selected < ex->item_count) {
                g_menu_ex = ex;
                g_menu_item_idx = ex->selected;
                action_rename();
            }
            return 1;

        default:
            return 0;
    }
}

int explorer_handle_mouse(explorer_t *ex, window_t *win,
                          int mx, int my, int lc, int rc) {
    if (!win->visible) return 0;

    ex->btn_back_hover      = tbtn_hit(win, TBTN_BACK,    mx, my);
    ex->btn_up_hover        = tbtn_hit(win, TBTN_UP,      mx, my);
    ex->btn_refresh_hover   = tbtn_hit(win, TBTN_REFRESH, mx, my);
    ex->btn_newfile_hover   = tbtn_hit(win, TBTN_NEWFILE, mx, my);
    ex->btn_newfolder_hover = tbtn_hit(win, TBTN_NEWFLD,  mx, my);

    if (lc) {
        if (tbtn_hit(win, TBTN_BACK, mx, my)) {
            explorer_up(ex); desktop.needs_full_redraw = 1; return 1;
        }
        if (tbtn_hit(win, TBTN_UP, mx, my)) {
            explorer_up(ex); desktop.needs_full_redraw = 1; return 1;
        }
        if (tbtn_hit(win, TBTN_REFRESH, mx, my)) {
            explorer_refresh(ex); desktop.needs_full_redraw = 1; return 1;
        }
        if (tbtn_hit(win, TBTN_NEWFILE, mx, my)) {
            int parent = (ex->nav_depth > 0)
                ? ex->nav_stack[ex->nav_depth - 1].dir_idx : VFS_ROOT_PARENT;
            char name[VFS_NAME_MAX];
            int i = 0;
            const char *p = "File "; while (*p) name[i++] = *p++;
            char num[8]; ex_itoa(++ex->file_counter, num);
            const char *n = num; while (*n) name[i++] = *n++;
            name[i++] = '.'; name[i++] = 't'; name[i++] = 'x'; name[i++] = 't'; name[i] = 0;
            vfs_create_in(name, 0, parent);
            explorer_refresh(ex); desktop.needs_full_redraw = 1; return 1;
        }
        if (tbtn_hit(win, TBTN_NEWFLD, mx, my)) {
            int parent = (ex->nav_depth > 0)
                ? ex->nav_stack[ex->nav_depth - 1].dir_idx : VFS_ROOT_PARENT;
            char name[VFS_NAME_MAX];
            int i = 0;
            const char *p = "New Folder "; while (*p) name[i++] = *p++;
            char num[8]; ex_itoa(++ex->folder_counter, num);
            const char *n = num; while (*n) name[i++] = *n++;
            name[i] = 0;
            vfs_create_in(name, 1, parent);
            explorer_refresh(ex); desktop.needs_full_redraw = 1; return 1;
        }
    }

    {
        int sx = sidebar_x(win);
        int sy = sidebar_y(win);
        int sh = sidebar_h(win);

        ex->sidebar_hover = -1;
        if (mx >= sx && mx < sx + EXPLO_SIDEBAR_W && my >= sy && my < sy + sh) {
            for (int i = 0; i < EXPLO_SIDEBAR_ITEMS; i++) {
                int iy = sy + 24 + i * EXPLO_ITEM_H;
                if (my >= iy && my < iy + EXPLO_ITEM_H) {
                    ex->sidebar_hover = i;
                    if (lc) {
                        int is_root = (sidebar_targets[i] == NULL);
                        if (is_root) {
                            ex->nav_depth = 0;
                            explorer_refresh(ex);
                        } else {
                            int tidx = vfs_find(sidebar_targets[i]);
                            if (tidx >= 0 && vfs.entries[tidx].is_dir) {
                                ex->nav_depth = 0;
                                explorer_navigate(ex, tidx, sidebar_targets[i]);
                            }
                        }
                    }
                    break;
                }
            }
            desktop.needs_full_redraw = 1;
            return 1;
        }
    }

    {
        int cx  = content_x(win);
        int cy  = content_y(win);
        int cw  = content_w(win);
        int ch  = content_h(win);

        ex->hover = -1;
        if (mx >= cx && mx < cx + cw && my >= cy && my < cy + ch) {
            int row = (my - cy) / EXPLO_ITEM_H + ex->scroll;
            if (row >= 0 && row < ex->item_count) {
                ex->hover = row;
                if (lc) {
                    if (row == ex->selected) {
                        explorer_open_selected(ex);
                        desktop.needs_full_redraw = 1;
                    } else {
                        ex->selected = row;
                        desktop.needs_full_redraw = 1;
                    }
                }
                if (rc) {
                    ex->selected = row;
                    g_menu_ex = ex;
                    g_menu_item_idx = row;
                    menu_clear();
                    menu_add_item("Rename", action_rename);
                    menu_add_item("Delete", action_delete);
                    menu_show(mx, my);
                    desktop.needs_full_redraw = 1;
                    return 1;
                }
            } else if (lc) {
                ex->selected = -1;
                desktop.needs_full_redraw = 1;
            }
            desktop.needs_full_redraw = 1;
            return 1;
        }
    }

    return 0;
}

explorer_t *explorer_create(void) {
    explorer_t *ex = (explorer_t *)mm_alloc(sizeof(explorer_t));
    if (!ex) return 0;
    ex_memset(ex, 0, sizeof(explorer_t));
    ex->selected      = -1;
    ex->hover         = -1;
    ex->sidebar_hover = -1;
    ex->rename_target = -1;
    explorer_refresh(ex);
    return ex;
}

void explorer_destroy(explorer_t *ex) {
    if (ex) mm_free(ex);
}