#include "desktop.h"
#include "menu.h"
#include "input.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"
#include "../drivers/rtc.h"
#include "../fs/fs.h"
#include "../mm/alloc.h"

desktop_t desktop;

// ── RNG ───────────────────────────────────────────────────────────────────────
static uint32_t rng = 0xdeadbeef;
static uint32_t rng_next(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}

// ── Cursor ────────────────────────────────────────────────────────────────────
static const uint8_t cursor_bmp[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,1,1,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
};

// ── Icon grid ─────────────────────────────────────────────────────────────────
#define ICON_W       52
#define ICON_H       40
#define ICON_LABEL_H 10
#define ICON_CELL_W  80
#define ICON_CELL_H  70
#define ICON_GRID_X  16
#define ICON_GRID_Y  16

static int icon_at(int mx, int my) {
    if (!vfs.mounted) return -1;
    int max_cols = (fb.width - ICON_GRID_X*2) / ICON_CELL_W;
    if (max_cols < 1) max_cols = 1;
    int col=0, row=0, slot=0;
    for (int i=0; i<VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        int ix = ICON_GRID_X + col*ICON_CELL_W;
        int iy = ICON_GRID_Y + row*ICON_CELL_H;
        if (mx>=ix && mx<ix+ICON_W && my>=iy && my<iy+ICON_H+ICON_LABEL_H)
            return i;
        slot++; col=slot%max_cols; row=slot/max_cols;
    }
    return -1;
}

// ── Open file/folder ──────────────────────────────────────────────────────────
static void open_entry(int fs_idx) {
    vfs_entry_t *e = &vfs.entries[fs_idx];
    if (!e->used) return;

    // Check if already open
    for (int i=0; i<desktop.nwindows; i++) {
        window_t *w = &desktop.windows[i];
        int match=1;
        for (int j=0; j<TITLE_MAX; j++) {
            if (w->title[j]!=e->name[j]) { match=0; break; }
            if (!w->title[j]) break;
        }
        if (match && w->visible) { desktop.active_win=i; desktop.needs_full_redraw=1; return; }
    }

    if (e->is_dir) {
        desktop_add_window(120, 80, 300, 180, e->name);
        window_set_content(&desktop.windows[desktop.nwindows-1], "(Empty folder)", 14);
    } else {
        int idx = desktop_add_window(120, 80, 360, 240, e->name);
        if (idx>=0 && e->size_bytes>0) {
            void *buf = mm_alloc(e->size_bytes+1);
            if (buf) {
                uint32_t n = vfs_read(fs_idx, buf);
                ((char*)buf)[n]=0;
                window_set_content(&desktop.windows[idx],(char*)buf,n);
            }
        } else if (idx>=0) {
            window_set_content(&desktop.windows[idx],"(Empty file)",12);
        }
    }
    desktop.needs_full_redraw=1;
}

// ── Rename callbacks ──────────────────────────────────────────────────────────
static int rename_target = -1;

static void on_rename_confirm(const char *text, void *userdata) {
    (void)userdata;
    if (rename_target<0 || rename_target>=VFS_MAX_FILES) return;
    if (!vfs.entries[rename_target].used) return;
    if (!text || !text[0]) return;

    // Check no duplicate
    if (vfs_find(text)>=0) return;

    // Copy new name
    int i=0;
    while (text[i] && i<VFS_NAME_MAX-1) {
        vfs.entries[rename_target].name[i] = text[i]; i++;
    }
    vfs.entries[rename_target].name[i]=0;
    vfs_flush();

    rename_target=-1;
    desktop.needs_full_redraw=1;
}

static void on_rename_cancel(void *userdata) {
    (void)userdata;
    rename_target=-1;
    desktop.needs_full_redraw=1;
}

// ── Menu actions ──────────────────────────────────────────────────────────────
static int file_counter=0, folder_counter=0;

static void itoa2(int n, char *buf) {
    if (n>=10) { buf[0]='0'+n/10; buf[1]='0'+n%10; buf[2]=0; }
    else       { buf[0]='0'+n;    buf[1]=0; }
}

static void action_new_file(void) {
    if (!vfs.mounted) return;
    char name[32]; char num[4]; itoa2(++file_counter, num);
    int i=0; const char *p="File "; while(*p) name[i++]=*p++;
    int j=0; while(num[j]) name[i++]=num[j++];
    name[i++]='.'; name[i++]='t'; name[i++]='x'; name[i++]='t'; name[i]=0;
    vfs_create(name,0);
    desktop.needs_full_redraw=1;
}

static void action_new_folder(void) {
    if (!vfs.mounted) return;
    char name[32]; char num[4]; itoa2(++folder_counter, num);
    int i=0; const char *p="Folder "; while(*p) name[i++]=*p++;
    int j=0; while(num[j]) name[i++]=num[j++];
    name[i]=0;
    vfs_create(name,1);
    desktop.needs_full_redraw=1;
}

static void action_refresh(void) { desktop.needs_full_redraw=1; }

// These capture the selected icon at menu-show time
static int menu_icon_target = -1;

static void action_open(void) {
    if (menu_icon_target>=0) open_entry(menu_icon_target);
}

static void action_rename(void) {
    if (menu_icon_target<0) return;
    rename_target=menu_icon_target;
    input_show("Rename",
               vfs.entries[rename_target].name,
               on_rename_confirm,
               on_rename_cancel,
               0);
    desktop.needs_full_redraw=1;
}

static void action_delete(void) {
    if (menu_icon_target<0) return;
    // Close any window showing this file
    for (int i=0; i<desktop.nwindows; i++) {
        window_t *w = &desktop.windows[i];
        int match=1;
        for (int j=0; j<TITLE_MAX; j++) {
            if (w->title[j]!=vfs.entries[menu_icon_target].name[j]) { match=0; break; }
            if (!w->title[j]) break;
        }
        if (match) w->visible=0;
    }
    vfs_delete(menu_icon_target);
    if (desktop.selected_icon==menu_icon_target) desktop.selected_icon=-1;
    menu_icon_target=-1;
    desktop.needs_full_redraw=1;
}

// ── Wallpaper ─────────────────────────────────────────────────────────────────
static void draw_wallpaper(void) {
    int dh=fb.height-TASKBAR_HEIGHT;
    fb_fill_gradient_v(0,0,fb.width,dh,COL_DESKTOP_TOP,COL_DESKTOP_BOT);
    int band_y=dh*2/5, band_h=dh/5;
    for (int i=0; i<band_h; i++) {
        uint8_t alpha=(i<band_h/2)?(uint8_t)(i*40/(band_h/2)):(uint8_t)((band_h-i)*40/(band_h/2));
        fb_fill_rect(0,band_y+i,fb.width,1,blend(COL_DESKTOP_BOT,0x2d1b69,alpha));
    }
    rng=0xdeadbeef;
    for (int i=0; i<260; i++) {
        int sx=rng_next()%fb.width, sy=rng_next()%dh;
        uint8_t br=60+(rng_next()%195);
        fb_putpixel(sx,sy,rgb(br,br,br+20));
    }
    for (int i=0; i<20; i++) {
        int sx=rng_next()%fb.width, sy=rng_next()%dh;
        fb_putpixel(sx,sy,0xffffff);
        if (sx>0) fb_putpixel(sx-1,sy,0x666688);
        if ((unsigned)(sx+1)<fb.width) fb_putpixel(sx+1,sy,0x666688);
        if (sy>0) fb_putpixel(sx,sy-1,0x666688);
        if (sy<dh-1) fb_putpixel(sx,sy+1,0x666688);
    }
}

// ── Icon grid ─────────────────────────────────────────────────────────────────
static void draw_icons(void) {
    if (!vfs.mounted) return;
    int max_cols=(fb.width-ICON_GRID_X*2)/ICON_CELL_W;
    if (max_cols<1) max_cols=1;
    int col=0,row=0,slot=0;
    for (int i=0; i<VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        vfs_entry_t *e=&vfs.entries[i];
        int ix=ICON_GRID_X+col*ICON_CELL_W;
        int iy=ICON_GRID_Y+row*ICON_CELL_H;

        if (i==desktop.selected_icon) {
            fb_fill_rect(ix-2,iy-2,ICON_W+4,ICON_H+ICON_LABEL_H+4,0x2a4a7a);
            fb_draw_rect(ix-2,iy-2,ICON_W+4,ICON_H+ICON_LABEL_H+4,0x4a7fa5);
        }

        if (e->is_dir) {
            fb_fill_rect(ix,iy+6,ICON_W,ICON_H-6,0x4a7fa5);
            fb_fill_rect(ix,iy+3,ICON_W/3,6,0x5a8fbf);
            fb_draw_rect(ix,iy+6,ICON_W,ICON_H-6,0x6aafdf);
        } else {
            fb_fill_rect(ix,iy,ICON_W-10,ICON_H,0x2a5a3a);
            fb_fill_rect(ix+ICON_W-10,iy+10,10,ICON_H-10,0x2a5a3a);
            fb_fill_rect(ix+ICON_W-10,iy,10,10,0x1a3a2a);
            fb_draw_hline(ix+ICON_W-10,iy+10,10,0x4a9a6a);
            fb_draw_vline(ix+ICON_W-10,iy,10,0x4a9a6a);
            fb_draw_hline(ix+6,iy+14,ICON_W-20,0x4a9a6a);
            fb_draw_hline(ix+6,iy+20,ICON_W-20,0x3a7a5a);
            fb_draw_hline(ix+6,iy+26,ICON_W-20,0x3a7a5a);
            fb_draw_rect(ix,iy,ICON_W-10,ICON_H,0x4a9a6a);
        }

        char label[16]; int li=0;
        while (e->name[li] && li<9) { label[li]=e->name[li]; li++; }
        if (e->name[li]) { label[li++]='.'; label[li++]='.'; }
        label[li]=0;
        int lw=text_strlen(label)*(GLYPH_W+1);
        text_puts(ix+(ICON_W-lw)/2, iy+ICON_H+2, label, 0xd8e8f8,0,1);

        slot++; col=slot%max_cols; row=slot/max_cols;
    }
}

// ── Taskbar ───────────────────────────────────────────────────────────────────
static void draw_taskbar(void) {
    int ty=fb.height-TASKBAR_HEIGHT;
    fb_fill_gradient_v(0,ty,fb.width,TASKBAR_HEIGHT,0x0e1628,COL_TASKBAR_BG);
    fb_draw_hline(0,ty,fb.width,COL_TASKBAR_BORDER);
    fb_draw_hline(0,ty+1,fb.width,0x1a2a40);
    int bh=TASKBAR_HEIGHT-8, by=ty+4;
    fb_fill_gradient_v(6,by,56,bh,0x5a8fbf,0x3a6f9f);
    fb_draw_rect(6,by,56,bh,0x7aafdf);
    text_puts_centered(6,by+(bh-GLYPH_H)/2,56,"Velox",0xffffff,0,1);
    int wx=70;
    for (int i=0; i<desktop.nwindows; i++) {
        window_t *w=&desktop.windows[i];
        if (!w->visible&&!w->minimized) continue;
        int active=(i==desktop.active_win);
        fb_fill_rect(wx,by,110,bh,active?COL_TASKBAR_BTN_ACT:COL_TASKBAR_BTN);
        fb_draw_rect(wx,by,110,bh,active?COL_TASKBAR_BORDER:0x2a3a5a);
        if (active) fb_fill_rect(wx+2,by+bh-3,106,2,COL_TASKBAR_BORDER);
        uint32_t dot=w->minimized?0x888888:(active?0x80c0ff:0x405070);
        fb_fill_rect(wx+6,by+bh/2-2,4,4,dot);
        text_puts(wx+14,by+(bh-GLYPH_H)/2,w->title,active?0xf0f0f0:0x8899bb,0,1);
        wx+=118;
    }
    int tray_w=90,tray_x=fb.width-tray_w-4;
    fb_fill_rect(tray_x,by,tray_w,bh,COL_TRAY_BG);
    fb_draw_rect(tray_x,by,tray_w,bh,0x2a3a5a);
    rtc_time_t t; rtc_read(&t);
    char timebuf[9]; rtc_format_time(&t,timebuf);
    text_puts_centered(tray_x,by+(bh-GLYPH_H)/2,tray_w,timebuf,COL_CLOCK,0,1);
}

static void draw_cursor_at(int mx, int my) {
    for (int r=0; r<CURSOR_H; r++)
        for (int c=0; c<CURSOR_W; c++) {
            int px=mx+c,py=my+r;
            if (px<0||py<0||px>=(int)fb.width||py>=(int)fb.height) continue;
            if      (cursor_bmp[r][c]==1) fb_putpixel(px,py,0x000000);
            else if (cursor_bmp[r][c]==2) fb_putpixel(px,py,0xffffff);
        }
}

// ── Public API ────────────────────────────────────────────────────────────────
void desktop_init(void) {
    desktop.nwindows=0; desktop.active_win=-1;
    desktop.mx=fb.width/2; desktop.my=fb.height/2;
    desktop.btn_left=0; desktop.btn_right=0;
    desktop.dirty=1; desktop.needs_full_redraw=1;
    desktop.cursor_saved=0; desktop.selected_icon=-1;
    menu_clear();
}

int desktop_add_window(int x, int y, int w, int h, const char *title) {
    if (desktop.nwindows>=MAX_WINDOWS) return -1;
    int idx=desktop.nwindows++;
    window_t *win=&desktop.windows[idx];
    win->x=x; win->y=y; win->w=w; win->h=h;
    win->visible=1; win->minimized=0; win->dragging=0;
    win->has_content=0; win->content[0]=0;
    int i=0; while(title[i]&&i<TITLE_MAX-1){win->title[i]=title[i];i++;} win->title[i]=0;
    desktop.active_win=idx; desktop.needs_full_redraw=1;
    return idx;
}

void desktop_redraw(void) {
    draw_wallpaper(); draw_icons();
    for (int i=0; i<desktop.nwindows; i++)
        window_draw(&desktop.windows[i],i==desktop.active_win);
    draw_taskbar(); menu_draw(); input_draw();

    int sx=desktop.mx,sy=desktop.my;
    if (sx+CURSOR_W>(int)fb.width)  sx=fb.width-CURSOR_W;
    if (sy+CURSOR_H>(int)fb.height) sy=fb.height-CURSOR_H;
    if (sx<0) sx=0; if (sy<0) sy=0;
    fb_save_region(sx,sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
    desktop.cursor_saved=1; desktop.cursor_sx=sx; desktop.cursor_sy=sy;
    draw_cursor_at(desktop.mx,desktop.my);
    fb_flip(); desktop.dirty=0; desktop.needs_full_redraw=0;
}

void desktop_update_cursor(void) {
    if (desktop.cursor_saved) {
        fb_restore_region(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
        fb_flip_rect(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H);
    }
    int sx=desktop.mx,sy=desktop.my;
    if (sx+CURSOR_W>(int)fb.width)  sx=fb.width-CURSOR_W;
    if (sy+CURSOR_H>(int)fb.height) sy=fb.height-CURSOR_H;
    if (sx<0) sx=0; if (sy<0) sy=0;
    fb_save_region(sx,sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
    desktop.cursor_sx=sx; desktop.cursor_sy=sy; desktop.cursor_saved=1;
    draw_cursor_at(desktop.mx,desktop.my);
    fb_flip_rect(sx,sy,CURSOR_W,CURSOR_H);
}

void desktop_mouse_move(int dx, int dy, int btn_left, int btn_right) {
    if (desktop.cursor_saved)
        fb_restore_region(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H,desktop.cursor_save);

    desktop.mx+=dx; desktop.my+=dy;
    if (desktop.mx<0) desktop.mx=0;
    if (desktop.my<0) desktop.my=0;
    if (desktop.mx>=(int)fb.width)  desktop.mx=fb.width-1;
    if (desktop.my>=(int)fb.height) desktop.my=fb.height-1;

    if (ctx_menu.visible) { menu_handle_hover(desktop.mx,desktop.my); desktop.needs_full_redraw=1; }
    if (input_box.visible) desktop.needs_full_redraw=1;

    int left_clicked  = btn_left  && !desktop.btn_left;
    int right_clicked = btn_right && !desktop.btn_right;
    desktop.btn_left=btn_left; desktop.btn_right=btn_right;

    // ── Right click ───────────────────────────────────────────────────────────
    if (right_clicked && !input_box.visible) {
        int icon=icon_at(desktop.mx,desktop.my);
        menu_clear();

        if (icon>=0) {
            // Right clicked on a file/folder
            menu_icon_target=icon;
            desktop.selected_icon=icon;
            menu_add_item("Open",   action_open);
            menu_add_separator();
            menu_add_item("Rename", action_rename);
            menu_add_item("Delete", action_delete);
        } else {
            // Right clicked on desktop background
            menu_icon_target=-1;
            int on_window=0;
            for (int i=0; i<desktop.nwindows; i++) {
                window_t *w=&desktop.windows[i];
                if (!w->visible) continue;
                if (desktop.mx>=w->x&&desktop.mx<w->x+w->w&&
                    desktop.my>=w->y&&desktop.my<w->y+w->h) { on_window=1; break; }
            }
            if (!on_window) {
                menu_add_item("New File",   action_new_file);
                menu_add_item("New Folder", action_new_folder);
                menu_add_separator();
                menu_add_item("Refresh",    action_refresh);
            }
        }
        if (ctx_menu.nitems>0) menu_show(desktop.mx,desktop.my);
        desktop.needs_full_redraw=1;
    }

    // ── Left click ────────────────────────────────────────────────────────────
    if (left_clicked) {
        if (input_box.visible) goto done;  // input box captures all clicks

        if (ctx_menu.visible) {
            menu_handle_click(desktop.mx,desktop.my);
            desktop.needs_full_redraw=1;
            goto done;
        }

        // Window buttons
        for (int i=desktop.nwindows-1; i>=0; i--) {
            window_t *w=&desktop.windows[i];
            if (!w->visible) continue;
            int hit=window_hit_button(w,desktop.mx,desktop.my);
            if (hit==BTN_CLOSE) { w->visible=0; desktop.needs_full_redraw=1; goto done; }
            if (hit==BTN_MIN)   { w->minimized=!w->minimized; w->visible=!w->minimized; desktop.needs_full_redraw=1; goto done; }
        }

        // Window drag/focus
        int clicked_window=0;
        for (int i=desktop.nwindows-1; i>=0; i--) {
            window_t *w=&desktop.windows[i];
            if (!w->visible) continue;
            if (desktop.mx>=w->x&&desktop.mx<w->x+w->w&&
                desktop.my>=w->y&&desktop.my<w->y+w->h) {
                desktop.active_win=i; clicked_window=1;
                if (desktop.my<w->y+TITLEBAR_HEIGHT) {
                    w->dragging=1;
                    w->drag_ox=desktop.mx-w->x;
                    w->drag_oy=desktop.my-w->y;
                }
                desktop.needs_full_redraw=1;
                goto done;
            }
        }

        // Icon click
        if (!clicked_window) {
            int icon=icon_at(desktop.mx,desktop.my);
            if (icon>=0) {
                if (icon==desktop.selected_icon) {
                    open_entry(icon);
                    desktop.selected_icon=-1;
                } else {
                    desktop.selected_icon=icon;
                    desktop.needs_full_redraw=1;
                }
            } else {
                desktop.selected_icon=-1;
                desktop.needs_full_redraw=1;
            }
        }
    }

done:
    if (!btn_left)
        for (int i=0; i<desktop.nwindows; i++) desktop.windows[i].dragging=0;

    if (btn_left) {
        for (int i=0; i<desktop.nwindows; i++) {
            window_t *w=&desktop.windows[i];
            if (w->dragging) {
                int nx=desktop.mx-w->drag_ox, ny=desktop.my-w->drag_oy;
                if (ny<0) ny=0;
                if (ny+w->h>(int)fb.height-TASKBAR_HEIGHT) ny=fb.height-TASKBAR_HEIGHT-w->h;
                if (nx!=w->x||ny!=w->y) { w->x=nx; w->y=ny; desktop.needs_full_redraw=1; }
            }
        }
    }

    desktop.dirty=1;
}