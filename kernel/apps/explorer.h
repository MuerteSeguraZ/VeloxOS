#pragma once
#include "../stdint.h"
#include "../fs/fs.h"
#include "../drivers/keyboard/keyboard.h"
#include "../ui/window.h"

#define EXPLORER_W          520
#define EXPLORER_H          360
#define EXPLORER_DEFAULT_X  60
#define EXPLORER_DEFAULT_Y  40

#define EXPLO_SIDEBAR_W     130
#define EXPLO_TOOLBAR_H     26
#define EXPLO_STATUSBAR_H   16
#define EXPLO_ITEM_H        18

#define EXPLO_MAX_PATH_DEPTH  8
#define EXPLO_SIDEBAR_ITEMS   3

#define COL_EXPLO_SIDEBAR_BG  0x0c0c18
#define COL_EXPLO_SIDEBAR_BD  0x2a3a5a
#define COL_EXPLO_CONTENT_BG  0x0e0e1a
#define COL_EXPLO_CONTENT_BG2 0x111122
#define COL_EXPLO_TOOLBAR_BG  0x111120
#define COL_EXPLO_TOOLBAR_BD  0x2a3a5a
#define COL_EXPLO_STATUS_BG   0x0a0a16
#define COL_EXPLO_STATUS_FG   0x506070
#define COL_EXPLO_SEL_BG      0x1e3a5f
#define COL_EXPLO_SEL_BD      0x4a7fa5
#define COL_EXPLO_HOVER_BG    0x15253a
#define COL_EXPLO_TEXT        0xc8d8e8
#define COL_EXPLO_TEXT_DIM    0x607080
#define COL_EXPLO_TEXT_DIR    0x7ab8e8
#define COL_EXPLO_TEXT_FILE   0xa8c8a8
#define COL_EXPLO_ICON_FILE   0x3a7a5a
#define COL_EXPLO_BREADCRUMB  0x8899bb
#define COL_EXPLO_BTN_BG      0x1a2a40
#define COL_EXPLO_BTN_BD      0x2a4a6a
#define COL_EXPLO_BTN_HOV     0x2a3a5a

typedef struct {
    int  dir_idx;
    char name[VFS_NAME_MAX];
} explo_nav_entry_t;

typedef struct {
    int      fs_idx;
    char     name[VFS_NAME_MAX];
    int      is_dir;
    uint32_t size_bytes;
} explo_item_t;

typedef struct {
    explo_nav_entry_t nav_stack[EXPLO_MAX_PATH_DEPTH];
    int               nav_depth;

    explo_item_t      items[VFS_MAX_FILES];
    int               item_count;
    int               selected;
    int               scroll;
    int               hover;
    int               rename_target;
    int               sidebar_hover;
    int               btn_back_hover;
    int               btn_up_hover;
    int               btn_refresh_hover;
    int               btn_newfile_hover;
    int               btn_newfolder_hover;
    int               btn_delete_hover;
    int               needs_refresh;
    void             *window_node;
    int               file_counter;
    int               folder_counter;
} explorer_t;

explorer_t *explorer_create(void);
void        explorer_destroy(explorer_t *ex);
void        explorer_draw(explorer_t *ex, window_t *win, int active);
int         explorer_handle_key(explorer_t *ex, window_t *win,
                                const key_event_t *evt);
int         explorer_handle_mouse(explorer_t *ex, window_t *win,
                                  int mx, int my, int lc, int rc);
void        explorer_refresh(explorer_t *ex);
void        explorer_navigate(explorer_t *ex, int dir_idx, const char *name);
void        explorer_up(explorer_t *ex);
void        explorer_open_selected(explorer_t *ex);