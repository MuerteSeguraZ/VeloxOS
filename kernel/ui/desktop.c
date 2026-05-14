#include "desktop.h"
#include "menu.h"
#include "input.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"
#include "../drivers/rtc/rtc.h"
#include "../fs/fs.h"
#include "../mm/alloc.h"
#include "../apps/explorer.h"

desktop_t desktop;

// ── RNG ───────────────────────────────────────────────────────────────────────
static uint32_t rng = 0xdeadbeef;
static uint32_t rng_next(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}

// ── Cursor ────────────────────────────────────────────────────────────────────
static const uint8_t cursor_bmp[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},{1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},{1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},{1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,1,1,1,0,0,0,0,0},{1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
};

static int selected_icon_ctx = VFS_ROOT_PARENT;

// ── Icon grid positions ───────────────────────────────────────────────────────
static int icon_col[VFS_MAX_FILES];
static int icon_row[VFS_MAX_FILES];

// ── Icon drag state ───────────────────────────────────────────────────────────
static int icon_dragging          = 0;
static int drag_icon_idx          = -1;
static int drag_px                = 0;
static int drag_py                = 0;
static int drag_ox                = 0;
static int drag_oy                = 0;
static int drag_moved             = 0;
static int icon_drag_was_selected = 0;

// ── Special desktop icons ─────────────────────────────────────────────────────
#define DESKTOP_EXPLORER_IDX  99999

// ── Window overlap detection ──────────────────────────────────────────────────
// ── Forward declarations ──────────────────────────────────────────────────────
static void cell_to_px(int c, int r, int *px, int *py);
static void icon_get_pos(int idx, int *ox, int *oy);
static void icons_assign_slots(void);

static int windows_overlap(int x1, int y1, int w1, int h1,
                          int x2, int y2, int w2, int h2) {
    return !(x1 + w1 <= x2 || x2 + w2 <= x1 ||
             y1 + h1 <= y2 || y2 + h2 <= y1);
}

static int window_position_free(int x, int y, int w, int h) {
    // Check against existing windows
    for (window_node_t *node = desktop.windows; node; node = node->next) {
        if (windows_overlap(x, y, w, h, 
                           node->win->x, node->win->y, 
                           node->win->w, node->win->h)) {
            return 0;  // Overlaps with window
        }
    }
    
    // Check against Explorer icon
    int explorer_ix = ICON_GRID_X + ICON_CELL_W;  // Second column
    int explorer_iy = ICON_GRID_Y;                  // Same row
    if (windows_overlap(x, y, w, h, explorer_ix, explorer_iy, 
                       ICON_W, ICON_H + ICON_LABEL_H)) {
        return 0;  // Overlaps with Explorer icon
    }
    
    // Check against desktop file icons
    if (vfs.mounted) {
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            if (!vfs.entries[i].used) continue;
            if ((int)vfs.entries[i].parent_idx != VFS_ROOT_PARENT) continue;
            int ix, iy;
            icon_get_pos(i, &ix, &iy);
            if (windows_overlap(x, y, w, h, ix, iy, ICON_W, ICON_H + ICON_LABEL_H)) {
                return 0;  // Overlaps with icon
            }
        }
    }
    
    return 1;  // Free
}

static void find_free_window_pos(int req_x, int req_y, int w, int h,
                                int *out_x, int *out_y) {
    // Try requested position first
    if (window_position_free(req_x, req_y, w, h)) {
        *out_x = req_x;
        *out_y = req_y;
        return;
    }

    // Try positions: cascade down and right
    int offsets[][2] = {
        {0, 60},      // Below
        {60, 0},      // Right
        {60, 60},     // Down-right
        {120, 0},     // Further right
        {0, 120},     // Further down
        {120, 120},   // Further down-right
        {180, 0},     // Way right
        {0, 180},     // Way down
        {-60, 0},     // Left
        {0, -60},     // Up
    };

    for (int i = 0; i < 10; i++) {
        int try_x = req_x + offsets[i][0];
        int try_y = req_y + offsets[i][1];

        // Keep window on screen
        if (try_x < 0) try_x = 0;
        if (try_y < 0) try_y = 0;
        if (try_x + w > (int)fb.width) try_x = fb.width - w - 10;
        if (try_y + h > (int)fb.height - TASKBAR_HEIGHT) 
            try_y = fb.height - TASKBAR_HEIGHT - h - 10;

        if (window_position_free(try_x, try_y, w, h)) {
            *out_x = try_x;
            *out_y = try_y;
            return;
        }
    }

    // Fallback: cascade positions if everything else is taken
    *out_x = 40;
    *out_y = 40;
}

static int grid_cols(void) {
    int mc = (fb.width - ICON_GRID_X * 2) / ICON_CELL_W;
    return mc < 1 ? 1 : mc;
}

static int cell_taken(int c, int r, int exclude) {
    // Check if Explorer icon occupies this cell (Explorer is at column 1, row 0)
    if (c == 1 && r == 0) return 1;
    
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (i == exclude) continue;
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != VFS_ROOT_PARENT) continue;
        if (icon_col[i] == c && icon_row[i] == r) return 1;
    }
    return 0;
}

static void icons_assign_slots(void) {
    int mc = grid_cols();
    int slot = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != VFS_ROOT_PARENT) continue;
        if (icon_col[i] >= 0) continue;
        while (cell_taken(slot % mc, slot / mc, i)) slot++;
        icon_col[i] = slot % mc;
        icon_row[i] = slot / mc;
        slot++;
    }
}

static void cell_to_px(int c, int r, int *px, int *py) {
    *px = ICON_GRID_X + c * ICON_CELL_W;
    *py = ICON_GRID_Y + r * ICON_CELL_H;
}

static void icon_get_pos(int idx, int *ox, int *oy) {
    if (icon_dragging && idx == drag_icon_idx) { *ox = drag_px; *oy = drag_py; return; }
    icons_assign_slots();
    cell_to_px(icon_col[idx], icon_row[idx], ox, oy);
}



static int snap_to_grid(int px, int py, int exclude, int *out_c, int *out_r) {
    int mc = grid_cols();
    int best_c = -1, best_r = -1, best_d = 0x7fffffff;
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < mc; c++) {
            if (cell_taken(c, r, exclude)) continue;
            int cx, cy; cell_to_px(c, r, &cx, &cy);
            int dx = px - cx, dy = py - cy;
            int d = dx * dx + dy * dy;
            if (d < best_d) { best_d = d; best_c = c; best_r = r; }
        }
    }
    if (best_c < 0) return 0;
    *out_c = best_c; *out_r = best_r;
    return 1;
}

static int icon_at(int mx, int my) {
    if (!vfs.mounted) return -1;
    
    // Check Explorer desktop icon first (top-left corner)
    int explorer_ix = ICON_GRID_X + ICON_CELL_W;
    int explorer_iy = ICON_GRID_Y;
    if (mx >= explorer_ix && mx < explorer_ix + ICON_W && 
        my >= explorer_iy && my < explorer_iy + ICON_H + ICON_LABEL_H) {
        return DESKTOP_EXPLORER_IDX;
    }
    
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != VFS_ROOT_PARENT) continue;
        int ix, iy; icon_get_pos(i, &ix, &iy);
        if (mx >= ix && mx < ix + ICON_W && my >= iy && my < iy + ICON_H + ICON_LABEL_H) return i;
    }
    return -1;
}

static int icon_at_in_folder(int mx, int my, int wx, int wy, int ww, int parent_idx) {
    if (!vfs.mounted) return -1;
    int mc = (ww - ICON_GRID_X * 2) / ICON_CELL_W; if (mc < 1) mc = 1;
    int col = 0, row = 0, slot = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != parent_idx) continue;
        int ix = wx + ICON_GRID_X + col * ICON_CELL_W;
        int iy = wy + TITLEBAR_HEIGHT + ICON_GRID_Y + row * ICON_CELL_H;
        if (mx >= ix && mx < ix + ICON_W && my >= iy && my < iy + ICON_H + ICON_LABEL_H) return i;
        slot++; col = slot % mc; row = slot / mc;
    }
    return -1;
}

// ── Pending close ─────────────────────────────────────────────────────────────
static window_node_t *pending_close_node = NULL;

static void do_close_window(window_node_t *node) {
    if (!node) return;
    // Destroy explorer instance if present
    if (node->win->explorer) {
        explorer_destroy(node->win->explorer);
        node->win->explorer = NULL;
    }
    if (desktop.windows == node) {
        desktop.windows = node->next;
    } else {
        window_node_t *curr = desktop.windows;
        while (curr && curr->next != node) curr = curr->next;
        if (curr) curr->next = node->next;
    }
    if (desktop.active_win == node) desktop.active_win = NULL;
    mm_free(node->win);
    mm_free(node);
    desktop.nwindows--;
    desktop.needs_full_redraw = 1;
}

static void on_save_yes(const char *text, void *ud) {
    (void)text;
    window_node_t *node = (window_node_t *)ud;
    if (!node || !node->win) return;
    window_t *w = node->win;
    if (w->fs_idx >= 0) {
        uint32_t len = 0; while (w->edit_buf[len]) len++;
        vfs_write(w->fs_idx, w->edit_buf, len);
    }
    do_close_window(node);
}

static void on_save_no(void *ud) {
    window_node_t *node = (window_node_t *)ud;
    if (!node || !node->win) return;
    window_t *w = node->win;
    int i = 0;
    while (w->orig_buf[i]) { w->edit_buf[i] = w->orig_buf[i]; i++; }
    w->edit_buf[i] = 0; w->edit_len = i; w->edit_dirty = 0;
    do_close_window(node);
}

static void on_save_cancel(void *ud) {
    (void)ud;
    pending_close_node = NULL;
    desktop.needs_full_redraw = 1;
}

static void try_close_window(window_node_t *node) {
    if (!node || !node->win) return;
    window_t *w = node->win;
    if (w->editable && w->edit_dirty) {
        pending_close_node = node;
        input_show_ync("Unsaved Changes",
                       "Save before closing?",
                       on_save_yes, on_save_no, on_save_cancel,
                       (void *)node);
        desktop.needs_full_redraw = 1;
    } else {
        do_close_window(node);
    }
}

// ── Open file/folder ──────────────────────────────────────────────────────────
static void open_entry(int fs_idx) {
    vfs_entry_t *e = &vfs.entries[fs_idx];
    if (!e->used) return;

    // Already open?
    for (window_node_t *node = desktop.windows; node; node = node->next) {
        window_t *w = node->win;
        int m = 1;
        for (int j = 0; j < TITLE_MAX; j++) {
            if (w->title[j] != e->name[j]) { m = 0; break; }
            if (!w->title[j]) break;
        }
        if (m && w->visible) { desktop.active_win = node; desktop.needs_full_redraw = 1; return; }
    }

    if (e->is_dir) {
        // Open folders using the Explorer app
        int final_x, final_y;
        find_free_window_pos(EXPLORER_DEFAULT_X, EXPLORER_DEFAULT_Y,
                            EXPLORER_W, EXPLORER_H, &final_x, &final_y);
        window_node_t *node = desktop_add_window(final_x, final_y,
                                                EXPLORER_W, EXPLORER_H, e->name);
        if (node) {
            explorer_t *ex = explorer_create();
            if (ex) {
                node->win->explorer = ex;
                ex->window_node = node;
                explorer_navigate(ex, fs_idx, e->name);
            }
        }
    } else {
        window_node_t *node = desktop_add_window(120, 80, 400, 280, e->name);
        if (node) {
            if (e->size_bytes > 0) {
                void *buf = mm_alloc(e->size_bytes + 1);
                if (buf) {
                    uint32_t n = vfs_read(fs_idx, buf);
                    ((char *)buf)[n] = 0;
                    window_set_editable(node->win, (char *)buf, n, fs_idx);
                    mm_free(buf);
                }
            } else {
                window_set_editable(node->win, "", 0, fs_idx);
            }
        }
    }
    desktop.needs_full_redraw = 1;
}

// ── Menu actions ──────────────────────────────────────────────────────────────
static int file_counter = 0, folder_counter = 0;
static void itoa2(int n, char *b) {
    if (n >= 10) { b[0] = '0' + n / 10; b[1] = '0' + n % 10; b[2] = 0; }
    else         { b[0] = '0' + n; b[1] = 0; }
}

static int menu_icon_target = -1;
static void action_open(void) { if (menu_icon_target >= 0) open_entry(menu_icon_target); }

static int rclick_col = -1;
static int rclick_row = -1;

static void place_new_icon(const char *name) {
    int idx = vfs_find(name);
    if (idx < 0) return;
    if (rclick_col >= 0) {
        int tc = rclick_col, tr = rclick_row;
        if (cell_taken(tc, tr, idx)) {
            int px, py; cell_to_px(tc, tr, &px, &py);
            snap_to_grid(px, py, idx, &tc, &tr);
        }
        icon_col[idx] = tc;
        icon_row[idx] = tr;
    }
}

static void action_new_file(void) {
    if (!vfs.mounted) return;
    char name[32]; char num[4]; itoa2(++file_counter, num);
    int i = 0; const char *p = "File "; while (*p) name[i++] = *p++;
    int j = 0; while (num[j]) name[i++] = num[j++];
    name[i++] = '.'; name[i++] = 't'; name[i++] = 'x'; name[i++] = 't'; name[i] = 0;
    vfs_create(name, 0); place_new_icon(name); desktop.needs_full_redraw = 1;
}
static void action_new_folder(void) {
    if (!vfs.mounted) return;
    char name[32]; char num[4]; itoa2(++folder_counter, num);
    int i = 0; const char *p = "Folder "; while (*p) name[i++] = *p++;
    int j = 0; while (num[j]) name[i++] = num[j++]; name[i] = 0;
    vfs_create(name, 1); place_new_icon(name); desktop.needs_full_redraw = 1;
}
static void action_refresh(void) { desktop.needs_full_redraw = 1; }

// ── Explorer launch action ────────────────────────────────────────────────────
static void action_open_explorer(void) {
    int final_x, final_y;
    find_free_window_pos(EXPLORER_DEFAULT_X, EXPLORER_DEFAULT_Y,
                        EXPLORER_W, EXPLORER_H, &final_x, &final_y);
    window_node_t *node = desktop_add_window(final_x, final_y,
                                            EXPLORER_W, EXPLORER_H, "Explorer");
    if (!node) return;
    explorer_t *ex = explorer_create();
    if (!ex) { do_close_window(node); return; }
    node->win->explorer = ex;
    ex->window_node = node;
    desktop.needs_full_redraw = 1;
}

// ── Folder-scoped new file / folder ──────────────────────────────────────────
static int active_folder_idx = -1;

static void action_new_file_in_folder(void) {
    if (!vfs.mounted || active_folder_idx < 0) return;
    char name[32]; char num[4]; itoa2(++file_counter, num);
    int i = 0; const char *p = "File "; while (*p) name[i++] = *p++;
    int j = 0; while (num[j]) name[i++] = num[j++];
    name[i++] = '.'; name[i++] = 't'; name[i++] = 'x'; name[i++] = 't'; name[i] = 0;
    vfs_create_in(name, 0, active_folder_idx); desktop.needs_full_redraw = 1;
}
static void action_new_folder_in_folder(void) {
    if (!vfs.mounted || active_folder_idx < 0) return;
    char name[32]; char num[4]; itoa2(++folder_counter, num);
    int i = 0; const char *p = "Folder "; while (*p) name[i++] = *p++;
    int j = 0; while (num[j]) name[i++] = num[j++]; name[i] = 0;
    vfs_create_in(name, 1, active_folder_idx); desktop.needs_full_redraw = 1;
}

static int rename_target = -1;
static void on_rename_confirm(const char *text, void *ud) {
    (void)ud;
    if (rename_target < 0 || !vfs.entries[rename_target].used || !text || !text[0]) return;
    if (vfs_find(text) >= 0) return;
    int i = 0;
    while (text[i] && i < VFS_NAME_MAX - 1) { vfs.entries[rename_target].name[i] = text[i]; i++; }
    vfs.entries[rename_target].name[i] = 0;
    vfs_flush(); rename_target = -1; desktop.needs_full_redraw = 1;
}
static void on_rename_cancel(void *ud) {
    (void)ud; rename_target = -1; desktop.needs_full_redraw = 1;
}

static void action_rename(void) {
    if (menu_icon_target < 0) return;
    rename_target = menu_icon_target;
    input_show("Rename", vfs.entries[rename_target].name,
               on_rename_confirm, on_rename_cancel, 0);
    desktop.needs_full_redraw = 1;
}
static void action_delete(void) {
    if (menu_icon_target < 0) return;
    for (window_node_t *node = desktop.windows; node; node = node->next) {
        window_t *w = node->win;
        int m = 1;
        for (int j = 0; j < TITLE_MAX; j++) {
            if (w->title[j] != vfs.entries[menu_icon_target].name[j]) { m = 0; break; }
            if (!w->title[j]) break;
        }
        if (m) w->visible = 0;
    }
    vfs_delete(menu_icon_target);
    if (desktop.selected_icon == menu_icon_target) desktop.selected_icon = -1;
    menu_icon_target = -1; desktop.needs_full_redraw = 1;
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
static void draw_wallpaper(void) {
    int dh = fb.height - TASKBAR_HEIGHT;
    fb_fill_gradient_v(0, 0, fb.width, dh, COL_DESKTOP_TOP, COL_DESKTOP_BOT);
    int band_y = dh * 2 / 5, band_h = dh / 5;
    for (int i = 0; i < band_h; i++) {
        uint8_t a = (i < band_h / 2) ? (uint8_t)(i * 40 / (band_h / 2)) : (uint8_t)((band_h - i) * 40 / (band_h / 2));
        fb_fill_rect(0, band_y + i, fb.width, 1, blend(COL_DESKTOP_BOT, 0x2d1b69, a));
    }
    rng = 0xdeadbeef;
    for (int i = 0; i < 260; i++) {
        int sx = rng_next() % fb.width, sy = rng_next() % dh;
        uint8_t br = 60 + (rng_next() % 195);
        fb_putpixel(sx, sy, rgb(br, br, br + 20));
    }
    for (int i = 0; i < 20; i++) {
        int sx = rng_next() % fb.width, sy = rng_next() % dh;
        fb_putpixel(sx, sy, 0xffffff);
        if (sx > 0) fb_putpixel(sx - 1, sy, 0x666688);
        if ((unsigned)(sx + 1) < fb.width) fb_putpixel(sx + 1, sy, 0x666688);
        if (sy > 0) fb_putpixel(sx, sy - 1, 0x666688);
        if (sy < dh - 1) fb_putpixel(sx, sy + 1, 0x666688);
    }
}

static void draw_folder_icons(int wx, int wy, int ww, int parent_idx) {
    if (!vfs.mounted) return;
    int mc = (ww - ICON_GRID_X * 2) / ICON_CELL_W; if (mc < 1) mc = 1;
    int col = 0, row = 0, slot = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != parent_idx) continue;
        vfs_entry_t *e = &vfs.entries[i];
        int ix = wx + ICON_GRID_X + col * ICON_CELL_W;
        int iy = wy + TITLEBAR_HEIGHT + ICON_GRID_Y + row * ICON_CELL_H;
        if (i == desktop.selected_icon && selected_icon_ctx == parent_idx) {
            fb_fill_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x2a4a7a);
            fb_draw_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x4a7fa5);
        }
        if (e->is_dir) {
            fb_fill_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x4a7fa5);
            fb_fill_rect(ix, iy + 3, ICON_W / 3, 6, 0x5a8fbf);
            fb_draw_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x6aafdf);
        } else {
            fb_fill_rect(ix, iy, ICON_W - 10, ICON_H, 0x2a5a3a);
            fb_fill_rect(ix + ICON_W - 10, iy + 10, 10, ICON_H - 10, 0x2a5a3a);
            fb_fill_rect(ix + ICON_W - 10, iy, 10, 10, 0x1a3a2a);
            fb_draw_hline(ix + ICON_W - 10, iy + 10, 10, 0x4a9a6a);
            fb_draw_vline(ix + ICON_W - 10, iy, 10, 0x4a9a6a);
            fb_draw_hline(ix + 6, iy + 14, ICON_W - 20, 0x4a9a6a);
            fb_draw_hline(ix + 6, iy + 20, ICON_W - 20, 0x3a7a5a);
            fb_draw_hline(ix + 6, iy + 26, ICON_W - 20, 0x3a7a5a);
            fb_draw_rect(ix, iy, ICON_W - 10, ICON_H, 0x4a9a6a);
        }
        char label[16]; int li = 0;
        while (e->name[li] && li < 9) { label[li] = e->name[li]; li++; }
        if (e->name[li]) { label[li++] = '.'; label[li++] = '.'; } label[li] = 0;
        int lw = text_strlen(label) * (8 + 1);
        text_puts(ix + (ICON_W - lw) / 2, iy + ICON_H + 2, label, 0xd8e8f8, 0, 1);
        slot++; col = slot % mc; row = slot / mc;
    }
}

static void draw_icons(void) {
    if (!vfs.mounted) return;
    {
        int ix = ICON_GRID_X + ICON_CELL_W;
        int iy = ICON_GRID_Y;
        if (DESKTOP_EXPLORER_IDX == desktop.selected_icon && selected_icon_ctx == VFS_ROOT_PARENT) {
            fb_fill_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x2a4a7a);
            fb_draw_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x4a7fa5);
        }
        fb_fill_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x4a7fa5);
        fb_fill_rect(ix, iy + 3, ICON_W / 3, 6, 0x5a8fbf);
        fb_draw_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x6aafdf);
        text_puts(ix + (ICON_W - 56) / 2, iy + ICON_H + 2, "Explorer", 0xd8e8f8, 0, 1);
    }
    
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != VFS_ROOT_PARENT) continue;
        vfs_entry_t *e = &vfs.entries[i];
        int ix, iy; icon_get_pos(i, &ix, &iy);
        if (i == desktop.selected_icon && selected_icon_ctx == VFS_ROOT_PARENT) {
            fb_fill_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x2a4a7a);
            fb_draw_rect(ix - 2, iy - 2, ICON_W + 4, ICON_H + ICON_LABEL_H + 4, 0x4a7fa5);
        }
        if (e->is_dir) {
            fb_fill_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x4a7fa5);
            fb_fill_rect(ix, iy + 3, ICON_W / 3, 6, 0x5a8fbf);
            fb_draw_rect(ix, iy + 6, ICON_W, ICON_H - 6, 0x6aafdf);
        } else {
            fb_fill_rect(ix, iy, ICON_W - 10, ICON_H, 0x2a5a3a);
            fb_fill_rect(ix + ICON_W - 10, iy + 10, 10, ICON_H - 10, 0x2a5a3a);
            fb_fill_rect(ix + ICON_W - 10, iy, 10, 10, 0x1a3a2a);
            fb_draw_hline(ix + ICON_W - 10, iy + 10, 10, 0x4a9a6a);
            fb_draw_vline(ix + ICON_W - 10, iy, 10, 0x4a9a6a);
            fb_draw_hline(ix + 6, iy + 14, ICON_W - 20, 0x4a9a6a);
            fb_draw_hline(ix + 6, iy + 20, ICON_W - 20, 0x3a7a5a);
            fb_draw_hline(ix + 6, iy + 26, ICON_W - 20, 0x3a7a5a);
            fb_draw_rect(ix, iy, ICON_W - 10, ICON_H, 0x4a9a6a);
        }
        char label[16]; int li = 0;
        while (e->name[li] && li < 9) { label[li] = e->name[li]; li++; }
        if (e->name[li]) { label[li++] = '.'; label[li++] = '.'; } label[li] = 0;
        int lw = text_strlen(label) * (8 + 1);
        text_puts(ix + (ICON_W - lw) / 2, iy + ICON_H + 2, label, 0xd8e8f8, 0, 1);
    }
}

static void draw_taskbar(void) {
    int ty = fb.height - TASKBAR_HEIGHT;
    fb_fill_gradient_v(0, ty, fb.width, TASKBAR_HEIGHT, 0x0e1628, COL_TASKBAR_BG);
    fb_draw_hline(0, ty, fb.width, COL_TASKBAR_BORDER);
    fb_draw_hline(0, ty + 1, fb.width, 0x1a2a40);
    int bh = TASKBAR_HEIGHT - 8, by = ty + 4;

    fb_fill_gradient_v(6, by, 56, bh, 0x5a8fbf, 0x3a6f9f);
    fb_draw_rect(6, by, 56, bh, 0x7aafdf);
    text_puts_centered(6, by + (bh - 8) / 2, 56, "Velox", 0xffffff, 0, 1);

    fb_fill_rect(66, by, 66, bh, COL_TASKBAR_BTN);
    fb_draw_rect(66, by, 66, bh, 0x2a3a5a);
    text_puts_centered(66, by + (bh - 8) / 2, 66, "Explorer", 0x8ab8e0, 0, 1);

    int wx = 140;
    for (window_node_t *node = desktop.windows; node; node = node->next) {
        window_t *w = node->win;
        if (!w->visible && !w->minimized) continue;
        int active = (node == desktop.active_win);
        fb_fill_rect(wx, by, 110, bh, active ? COL_TASKBAR_BTN_ACT : COL_TASKBAR_BTN);
        fb_draw_rect(wx, by, 110, bh, active ? COL_TASKBAR_BORDER : 0x2a3a5a);
        if (active) fb_fill_rect(wx + 2, by + bh - 3, 106, 2, COL_TASKBAR_BORDER);
        if (w->edit_dirty) fb_fill_rect(wx + 4, by + bh / 2 - 2, 4, 4, 0xe0a030);
        else {
            uint32_t dot = w->minimized ? 0x888888 : (active ? 0x80c0ff : 0x405070);
            if (w->explorer) dot = active ? 0x5ab8ff : 0x2a5a8a;
            fb_fill_rect(wx + 4, by + bh / 2 - 2, 4, 4, dot);
        }
        text_puts(wx + 14, by + (bh - 8) / 2, w->title, active ? 0xf0f0f0 : 0x8899bb, 0, 1);
        wx += 118;
    }

    int tw = 90, tx = fb.width - tw - 4;
    fb_fill_rect(tx, by, tw, bh, COL_TRAY_BG);
    fb_draw_rect(tx, by, tw, bh, 0x2a3a5a);
    rtc_time_t t; rtc_read(&t); char tb[9]; rtc_format_time(&t, tb);
    text_puts_centered(tx, by + (bh - 8) / 2, tw, tb, COL_CLOCK, 0, 1);
}

static void draw_cursor_at(int mx, int my) {
    for (int r = 0; r < CURSOR_H; r++) for (int c = 0; c < CURSOR_W; c++) {
        int px = mx + c, py = my + r;
        if (px < 0 || py < 0 || px >= (int)fb.width || py >= (int)fb.height) continue;
        if (cursor_bmp[r][c] == 1) fb_putpixel(px, py, 0x000000);
        else if (cursor_bmp[r][c] == 2) fb_putpixel(px, py, 0xffffff);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void desktop_init(void) {
    desktop.windows   = NULL;
    desktop.nwindows  = 0;
    desktop.active_win = NULL;
    desktop.mx = fb.width / 2; desktop.my = fb.height / 2;
    desktop.btn_left = 0; desktop.btn_right = 0;
    desktop.dirty = 1; desktop.needs_full_redraw = 1;
    desktop.cursor_saved = 0; desktop.selected_icon = -1;
    selected_icon_ctx = VFS_ROOT_PARENT;
    icon_dragging = 0; drag_icon_idx = -1; drag_moved = 0; icon_drag_was_selected = 0;
    rclick_col = -1; rclick_row = -1;
    for (int i = 0; i < VFS_MAX_FILES; i++) { icon_col[i] = -1; icon_row[i] = -1; }
    menu_clear();
}

window_node_t *desktop_add_window(int x, int y, int w, int h, const char *title) {
    window_t *win = (window_t *)mm_alloc(sizeof(window_t));
    if (!win) return NULL;
    window_node_t *node = (window_node_t *)mm_alloc(sizeof(window_node_t));
    if (!node) { mm_free(win); return NULL; }
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->visible = 1; win->minimized = 0; win->dragging = 0;
    win->has_content = 0; win->content[0] = 0;
    win->editable = 0; win->edit_dirty = 0; win->edit_len = 0;
    win->folder_idx = -1;
    win->fs_idx = -1; win->scroll_row = 0;
    win->explorer = NULL;
    int i = 0; while (title[i] && i < TITLE_MAX - 1) { win->title[i] = title[i]; i++; } win->title[i] = 0;
    node->win  = win;
    node->next = NULL;
    if (!desktop.windows) {
        desktop.windows = node;
    } else {
        window_node_t *curr = desktop.windows;
        while (curr->next) curr = curr->next;
        curr->next = node;
    }
    desktop.nwindows++;
    desktop.active_win = node;
    desktop.needs_full_redraw = 1;
    return node;
}

void desktop_redraw(void) {
    draw_wallpaper();
    draw_icons();
    for (window_node_t *node = desktop.windows; node; node = node->next) {
        window_t *w = node->win;
        int is_active = (node == desktop.active_win);
        window_draw(w, is_active);
        if (w->visible) {
            if (w->explorer) {
                explorer_draw(w->explorer, w, is_active);
            } else if (w->folder_idx >= 0) {
                draw_folder_icons(w->x, w->y, w->w, w->folder_idx);
            }
        }
    }
    draw_taskbar();
    menu_draw();
    input_draw();
    int sx = desktop.mx, sy = desktop.my;
    if (sx + CURSOR_W > (int)fb.width)  sx = fb.width  - CURSOR_W;
    if (sy + CURSOR_H > (int)fb.height) sy = fb.height - CURSOR_H;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    fb_save_region(sx, sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
    desktop.cursor_saved = 1; desktop.cursor_sx = sx; desktop.cursor_sy = sy;
    draw_cursor_at(desktop.mx, desktop.my);
    fb_flip();
    desktop.dirty = 0; desktop.needs_full_redraw = 0;
}

void desktop_update_cursor(void) {
    if (desktop.cursor_saved) {
        fb_restore_region(desktop.cursor_sx, desktop.cursor_sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
        fb_flip_rect(desktop.cursor_sx, desktop.cursor_sy, CURSOR_W, CURSOR_H);
    }
    int sx = desktop.mx, sy = desktop.my;
    if (sx + CURSOR_W > (int)fb.width)  sx = fb.width  - CURSOR_W;
    if (sy + CURSOR_H > (int)fb.height) sy = fb.height - CURSOR_H;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    fb_save_region(sx, sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
    desktop.cursor_sx = sx; desktop.cursor_sy = sy; desktop.cursor_saved = 1;
    draw_cursor_at(desktop.mx, desktop.my);
    fb_flip_rect(sx, sy, CURSOR_W, CURSOR_H);
}

// ── Keyboard routing ──────────────────────────────────────────────────────────
void desktop_handle_key(const key_event_t *evt) {
    if (!evt || !evt->pressed) return;
    if (input_box.visible) { input_handle_key(evt); input_box.dirty = 1; return; }
    if (desktop.active_win) {
        window_t *w = desktop.active_win->win;
        if (w->explorer) {
            if (explorer_handle_key(w->explorer, w, evt))
                desktop.needs_full_redraw = 1;
            return;
        }
        if (window_handle_key(w, evt))
            desktop.needs_full_redraw = 1;
    }
}

void desktop_mouse_move(int dx, int dy, int btn_left, int btn_right) {
    if (desktop.cursor_saved)
        fb_restore_region(desktop.cursor_sx, desktop.cursor_sy, CURSOR_W, CURSOR_H, desktop.cursor_save);
    desktop.mx += dx; desktop.my += dy;
    if (desktop.mx < 0) desktop.mx = 0;
    if (desktop.my < 0) desktop.my = 0;
    if (desktop.mx >= (int)fb.width)  desktop.mx = fb.width  - 1;
    if (desktop.my >= (int)fb.height) desktop.my = fb.height - 1;

    if (ctx_menu.visible) { menu_handle_hover(desktop.mx, desktop.my); desktop.needs_full_redraw = 1; }
    if (input_box.visible) desktop.needs_full_redraw = 1;

    int lc = btn_left  && !desktop.btn_left;
    int rc = btn_right && !desktop.btn_right;
    desktop.btn_left  = btn_left;
    desktop.btn_right = btn_right;

    if (lc && !input_box.visible && !ctx_menu.visible) {
        int ty = fb.height - TASKBAR_HEIGHT;
        int bh = TASKBAR_HEIGHT - 8, by = ty + 4;
        if (desktop.mx >= 66 && desktop.mx < 132 &&
            desktop.my >= by && desktop.my < by + bh) {
            action_open_explorer();
            goto done;
        }
    }

    if (rc && !input_box.visible) {
        int icon = icon_at(desktop.mx, desktop.my);
        menu_clear();
        
        if (icon == DESKTOP_EXPLORER_IDX) {
            menu_add_item("Open", action_open_explorer);
            if (ctx_menu.nitems > 0) menu_show(desktop.mx, desktop.my);
            desktop.needs_full_redraw = 1;
            goto done;
        }
        
        if (icon >= 0) {
            menu_icon_target = icon; desktop.selected_icon = icon;
            selected_icon_ctx = VFS_ROOT_PARENT;
            menu_add_item("Open", action_open);
            menu_add_separator();
            menu_add_item("Rename", action_rename);
            menu_add_item("Delete", action_delete);
        } else {
            menu_icon_target = -1;
            int in_folder_win = 0;
            for (window_node_t *node = desktop.windows; node; node = node->next) {
                window_t *w = node->win;
                if (!w->visible) continue;
                if (desktop.mx >= w->x && desktop.mx < w->x + w->w &&
                    desktop.my >= w->y && desktop.my < w->y + w->h) {
                    if (w->explorer) { 
                        in_folder_win = 1;
                        explorer_handle_mouse(w->explorer, w, desktop.mx, desktop.my, 0, 1);
                        desktop.needs_full_redraw = 1;
                        break;
                    }
                    if (w->folder_idx >= 0) {
                        in_folder_win = 1;
                        int ficon = icon_at_in_folder(desktop.mx, desktop.my,
                                                      w->x, w->y, w->w, w->folder_idx);
                        if (ficon >= 0) {
                            menu_icon_target = ficon;
                            desktop.selected_icon = ficon;
                            selected_icon_ctx = w->folder_idx;
                            menu_add_item("Open", action_open);
                            menu_add_separator();
                            menu_add_item("Rename", action_rename);
                            menu_add_item("Delete", action_delete);
                        } else {
                            active_folder_idx = w->folder_idx;
                            menu_add_item("New File",   action_new_file_in_folder);
                            menu_add_item("New Folder", action_new_folder_in_folder);
                        }
                    }
                    break;
                }
            }
            if (!in_folder_win) {
                active_folder_idx = -1;
                int ow = 0;
                for (window_node_t *node = desktop.windows; node; node = node->next) {
                    window_t *w = node->win;
                    if (!w->visible) continue;
                    if (desktop.mx >= w->x && desktop.mx < w->x + w->w &&
                        desktop.my >= w->y && desktop.my < w->y + w->h) { ow = 1; break; }
                }
                if (!ow) {
                    int mc = grid_cols();
                    rclick_col = (desktop.mx - ICON_GRID_X) / ICON_CELL_W;
                    rclick_row = (desktop.my - ICON_GRID_Y) / ICON_CELL_H;
                    if (rclick_col < 0)   rclick_col = 0;
                    if (rclick_col >= mc) rclick_col = mc - 1;
                    if (rclick_row < 0)   rclick_row = 0;
                    menu_add_item("New File",   action_new_file);
                    menu_add_item("New Folder", action_new_folder);
                    menu_add_separator();
                    menu_add_item("Explorer",   action_open_explorer);
                    menu_add_separator();
                    menu_add_item("Refresh",    action_refresh);
                }
            }
        }
        if (ctx_menu.nitems > 0) menu_show(desktop.mx, desktop.my);
        desktop.needs_full_redraw = 1;
    }

    if (lc) {
        if (input_box.visible)  { input_handle_click(desktop.mx, desktop.my); desktop.needs_full_redraw = 1; goto done; }
        if (ctx_menu.visible)   { menu_handle_click(desktop.mx, desktop.my);  desktop.needs_full_redraw = 1; goto done; }
        {
            int tb_y  = (int)fb.height - TASKBAR_HEIGHT;
            int btn_h = TASKBAR_HEIGHT - 8;
            int btn_y = tb_y + 4;
            if (desktop.my >= btn_y && desktop.my < btn_y + btn_h) {
                if (desktop.mx >= 66 && desktop.mx < 132) {
                    action_open_explorer();
                    desktop.needs_full_redraw = 1;
                    goto done;
                }
                int wx = 140;
                for (window_node_t *node = desktop.windows; node; node = node->next) {
                    window_t *w = node->win;
                    if (!w->visible && !w->minimized) { wx += 118; continue; }
                    if (desktop.mx >= wx && desktop.mx < wx + 110) {
                        if (w->minimized) {
                            // Restore the window
                            w->minimized = 0;
                            w->visible   = 1;
                            desktop.active_win = node;
                        } else if (node == desktop.active_win) {
                            w->minimized = 1;
                            w->visible   = 0;
                            desktop.active_win = NULL;
                        } else {
                            desktop.active_win = node;
                        }
                        desktop.needs_full_redraw = 1;
                        goto done;
                    }
                    wx += 118;
                }

                goto done;
            }
        }

        window_node_t *topmost = NULL;
        for (window_node_t *node = desktop.windows; node; node = node->next) {
            window_t *w = node->win;
            if (!w->visible) continue;
            if (window_hit_button(w, desktop.mx, desktop.my)) { topmost = node; break; }
        }
        if (topmost) {
            window_t *w = topmost->win;
            int hit = window_hit_button(w, desktop.mx, desktop.my);
            if (hit == BTN_CLOSE) { try_close_window(topmost); goto done; }
            if (hit == BTN_MIN)   { w->minimized = !w->minimized; w->visible = !w->minimized; desktop.needs_full_redraw = 1; goto done; }
        }

        int cw = 0;
        for (window_node_t *node = desktop.windows; node; node = node->next) {
            window_t *w = node->win;
            if (!w->visible) continue;
            if (desktop.mx >= w->x && desktop.mx < w->x + w->w &&
                desktop.my >= w->y && desktop.my < w->y + w->h) {
                desktop.active_win = node; cw = 1;
                if (desktop.my < w->y + TITLEBAR_HEIGHT) {
                    w->dragging = 1;
                    w->drag_ox  = desktop.mx - w->x;
                    w->drag_oy  = desktop.my - w->y;
                } else if (w->explorer) {
                    explorer_handle_mouse(w->explorer, w, desktop.mx, desktop.my, 1, 0);
                } else if (w->folder_idx >= 0) {
                    int ficon = icon_at_in_folder(desktop.mx, desktop.my,
                                                  w->x, w->y, w->w, w->folder_idx);
                    if (ficon >= 0) {
                        if (ficon == desktop.selected_icon && selected_icon_ctx == w->folder_idx) {
                            open_entry(ficon);
                            desktop.selected_icon = -1;
                            selected_icon_ctx = VFS_ROOT_PARENT;
                        } else {
                            desktop.selected_icon = ficon;
                            selected_icon_ctx = w->folder_idx;
                        }
                    } else {
                        desktop.selected_icon = -1;
                        selected_icon_ctx = VFS_ROOT_PARENT;
                    }
                }
                desktop.needs_full_redraw = 1;
                goto done;
            }
        }

        if (!cw) {
            int icon = icon_at(desktop.mx, desktop.my);
            if (icon >= 0) {
                int already = (icon == desktop.selected_icon && selected_icon_ctx == VFS_ROOT_PARENT);
                if (icon == DESKTOP_EXPLORER_IDX) {
                    if (already) {
                        action_open_explorer();
                        desktop.selected_icon = -1;
                        selected_icon_ctx = VFS_ROOT_PARENT;
                    } else {
                        desktop.selected_icon = icon;
                        selected_icon_ctx = VFS_ROOT_PARENT;
                    }
                } else {
                    int ix, iy; icon_get_pos(icon, &ix, &iy);
                    icon_dragging       = 1;
                    drag_icon_idx       = icon;
                    drag_px = ix; drag_py = iy;
                    drag_ox = desktop.mx - ix;
                    drag_oy = desktop.my - iy;
                    drag_moved          = 0;
                    icon_drag_was_selected = already;
                    desktop.selected_icon = icon;
                    selected_icon_ctx   = VFS_ROOT_PARENT;
                }
                desktop.needs_full_redraw = 1;
            } else {
                icon_dragging = 0; drag_icon_idx = -1;
                desktop.selected_icon = -1;
                selected_icon_ctx = VFS_ROOT_PARENT;
                desktop.needs_full_redraw = 1;
            }
        }
    }

done:
    if (!btn_left) {
        for (window_node_t *node = desktop.windows; node; node = node->next)
            node->win->dragging = 0;
        if (icon_dragging) {
            if (!drag_moved && icon_drag_was_selected && drag_icon_idx >= 0) {
                open_entry(drag_icon_idx);
                desktop.selected_icon = -1;
                selected_icon_ctx = VFS_ROOT_PARENT;
                desktop.needs_full_redraw = 1;
            } else if (drag_moved && drag_icon_idx >= 0) {
                int nc, nr;
                if (snap_to_grid(drag_px, drag_py, drag_icon_idx, &nc, &nr)) {
                    icon_col[drag_icon_idx] = nc;
                    icon_row[drag_icon_idx] = nr;
                }
                desktop.needs_full_redraw = 1;
            }
            icon_dragging = 0; drag_icon_idx = -1; drag_moved = 0; icon_drag_was_selected = 0;
        }
    }

    if (btn_left) {
        for (window_node_t *node = desktop.windows; node; node = node->next) {
            window_t *w = node->win;
            if (!w->dragging) continue;
            int nx = desktop.mx - w->drag_ox, ny = desktop.my - w->drag_oy;
            if (ny < 0) ny = 0;
            if (ny + w->h > (int)fb.height - TASKBAR_HEIGHT) ny = fb.height - TASKBAR_HEIGHT - w->h;
            if (nx != w->x || ny != w->y) { w->x = nx; w->y = ny; desktop.needs_full_redraw = 1; }
        }
        if (desktop.active_win) {
            window_t *w = desktop.active_win->win;
            if (w->explorer && !w->dragging)
                explorer_handle_mouse(w->explorer, w, desktop.mx, desktop.my, 0, 0);
        }
        if (icon_dragging && drag_icon_idx >= 0) {
            int nx = desktop.mx - drag_ox;
            int ny = desktop.my - drag_oy;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + ICON_W > (int)fb.width)  nx = fb.width  - ICON_W;
            if (ny + ICON_H + ICON_LABEL_H > (int)fb.height - TASKBAR_HEIGHT)
                ny = fb.height - TASKBAR_HEIGHT - ICON_H - ICON_LABEL_H;
            if (nx != drag_px || ny != drag_py) {
                drag_px = nx; drag_py = ny;
                drag_moved = 1;
                desktop.needs_full_redraw = 1;
            }
        }
    }

    if (!btn_left && !lc && !rc && desktop.active_win) {
        window_t *w = desktop.active_win->win;
        if (w->explorer)
            explorer_handle_mouse(w->explorer, w, desktop.mx, desktop.my, 0, 0);
    }

    desktop.dirty = 1;
}