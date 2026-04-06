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
static uint32_t rng=0xdeadbeef;
static uint32_t rng_next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}

// ── Cursor ────────────────────────────────────────────────────────────────────
static const uint8_t cursor_bmp[CURSOR_H][CURSOR_W]={
    {1,0,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},{1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},{1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},{1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,1,1,1,0,0,0,0,0},{1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},{1,1,0,0,0,0,0,0,0,0,0,0},
};

// ── Icon grid ─────────────────────────────────────────────────────────────────
#define ICON_W       52
#define ICON_H       40
#define ICON_LABEL_H 10
#define ICON_CELL_W  80
#define ICON_CELL_H  70
#define ICON_GRID_X  16
#define ICON_GRID_Y  16

static int icon_at(int mx,int my){
    if(!vfs.mounted)return -1;
    int mc=(fb.width-ICON_GRID_X*2)/ICON_CELL_W; if(mc<1)mc=1;
    int col=0,row=0,slot=0;
    for(int i=0;i<VFS_MAX_FILES;i++){
        if(!vfs.entries[i].used)continue;
        int ix=ICON_GRID_X+col*ICON_CELL_W,iy=ICON_GRID_Y+row*ICON_CELL_H;
        if(mx>=ix&&mx<ix+ICON_W&&my>=iy&&my<iy+ICON_H+ICON_LABEL_H)return i;
        slot++;col=slot%mc;row=slot/mc;
    }
    return -1;
}

// ── Pending close (for unsaved changes dialog) ────────────────────────────────
static window_node_t *pending_close_node = NULL;

static void do_close_window(window_node_t *node){
    if(!node)return;
    
    // Unlink from list
    if(desktop.windows == node){
        desktop.windows = node->next;
    } else {
        window_node_t *curr = desktop.windows;
        while(curr && curr->next != node) curr = curr->next;
        if(curr) curr->next = node->next;
    }
    
    // Update active_win if needed
    if(desktop.active_win == node) desktop.active_win = NULL;
    
    // Free the window and node
    mm_free(node->win);
    mm_free(node);
    
    desktop.nwindows--;
    desktop.needs_full_redraw=1;
}

// YNC callbacks
static void on_save_yes(const char *text, void *ud){
    (void)text;
    window_node_t *node = (window_node_t *)ud;
    if(!node || !node->win) return;
    
    window_t *w = node->win;
    if(w->fs_idx>=0){
        uint32_t len=0; while(w->edit_buf[len])len++;
        vfs_write(w->fs_idx,w->edit_buf,len);
    }
    do_close_window(node);
}

static void on_save_no(void *ud){
    window_node_t *node = (window_node_t *)ud;
    if(!node || !node->win) return;
    
    window_t *w = node->win;
    int i=0;
    while(w->orig_buf[i]){w->edit_buf[i]=w->orig_buf[i];i++;}
    w->edit_buf[i]=0; w->edit_len=i; w->edit_dirty=0;
    do_close_window(node);
}

static void on_save_cancel(void *ud){
    (void)ud;
    pending_close_node=NULL;
    desktop.needs_full_redraw=1;
}

static void try_close_window(window_node_t *node){
    if(!node || !node->win) return;
    
    window_t *w = node->win;
    if(w->editable && w->edit_dirty){
        pending_close_node = node;
        input_show_ync("Unsaved Changes",
                       "Save before closing?",
                       on_save_yes,
                       on_save_no,
                       on_save_cancel,
                       (void*)node);
        desktop.needs_full_redraw=1;
    } else {
        do_close_window(node);
    }
}

// ── Open file/folder ──────────────────────────────────────────────────────────
static void open_entry(int fs_idx){
    vfs_entry_t *e=&vfs.entries[fs_idx];
    if(!e->used)return;

    // Already open?
    for(window_node_t *node=desktop.windows; node; node=node->next){
        window_t *w = node->win;
        int m=1;
        for(int j=0;j<TITLE_MAX;j++){if(w->title[j]!=e->name[j]){m=0;break;}if(!w->title[j])break;}
        if(m&&w->visible){desktop.active_win=node;desktop.needs_full_redraw=1;return;}
    }

    if(e->is_dir){
        window_node_t *node = desktop_add_window(120,80,300,180,e->name);
        if(node)
            window_set_content(node->win,"(Empty folder)",14);
    } else {
        window_node_t *node = desktop_add_window(120,80,400,280,e->name);
        if(node){
            if(e->size_bytes>0){
                void *buf=mm_alloc(e->size_bytes+1);
                if(buf){
                    uint32_t n=vfs_read(fs_idx,buf);
                    ((char*)buf)[n]=0;
                    window_set_editable(node->win,(char*)buf,n,fs_idx);
                    mm_free(buf);
                }
            } else {
                window_set_editable(node->win,"",0,fs_idx);
            }
        }
    }
    desktop.needs_full_redraw=1;
}

// ── Menu actions ──────────────────────────────────────────────────────────────
static int file_counter=0,folder_counter=0;
static void itoa2(int n,char *b){if(n>=10){b[0]='0'+n/10;b[1]='0'+n%10;b[2]=0;}else{b[0]='0'+n;b[1]=0;}}

static void action_new_file(void){
    if(!vfs.mounted)return;
    char name[32];char num[4];itoa2(++file_counter,num);
    int i=0;const char *p="File ";while(*p)name[i++]=*p++;
    int j=0;while(num[j])name[i++]=num[j++];
    name[i++]='.';name[i++]='t';name[i++]='x';name[i++]='t';name[i]=0;
    vfs_create(name,0);desktop.needs_full_redraw=1;
}
static void action_new_folder(void){
    if(!vfs.mounted)return;
    char name[32];char num[4];itoa2(++folder_counter,num);
    int i=0;const char *p="Folder ";while(*p)name[i++]=*p++;
    int j=0;while(num[j])name[i++]=num[j++];name[i]=0;
    vfs_create(name,1);desktop.needs_full_redraw=1;
}
static void action_refresh(void){desktop.needs_full_redraw=1;}

static int menu_icon_target=-1;
static void action_open(void){if(menu_icon_target>=0)open_entry(menu_icon_target);}

static int rename_target=-1;
static void on_rename_confirm(const char *text,void *ud){
    (void)ud;
    if(rename_target<0||!vfs.entries[rename_target].used||!text||!text[0])return;
    if(vfs_find(text)>=0)return;
    int i=0;while(text[i]&&i<VFS_NAME_MAX-1){vfs.entries[rename_target].name[i]=text[i];i++;}
    vfs.entries[rename_target].name[i]=0;
    vfs_flush();rename_target=-1;desktop.needs_full_redraw=1;
}
static void on_rename_cancel(void *ud){(void)ud;rename_target=-1;desktop.needs_full_redraw=1;}

static void action_rename(void){
    if(menu_icon_target<0)return;
    rename_target=menu_icon_target;
    input_show("Rename",vfs.entries[rename_target].name,
               on_rename_confirm,on_rename_cancel,0);
    desktop.needs_full_redraw=1;
}
static void action_delete(void){
    if(menu_icon_target<0)return;
    for(window_node_t *node=desktop.windows; node; node=node->next){
        window_t *w = node->win;
        int m=1;
        for(int j=0;j<TITLE_MAX;j++){if(w->title[j]!=vfs.entries[menu_icon_target].name[j]){m=0;break;}if(!w->title[j])break;}
        if(m)w->visible=0;
    }
    vfs_delete(menu_icon_target);
    if(desktop.selected_icon==menu_icon_target)desktop.selected_icon=-1;
    menu_icon_target=-1;desktop.needs_full_redraw=1;
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
static void draw_wallpaper(void){
    int dh=fb.height-TASKBAR_HEIGHT;
    fb_fill_gradient_v(0,0,fb.width,dh,COL_DESKTOP_TOP,COL_DESKTOP_BOT);
    int band_y=dh*2/5,band_h=dh/5;
    for(int i=0;i<band_h;i++){
        uint8_t a=(i<band_h/2)?(uint8_t)(i*40/(band_h/2)):(uint8_t)((band_h-i)*40/(band_h/2));
        fb_fill_rect(0,band_y+i,fb.width,1,blend(COL_DESKTOP_BOT,0x2d1b69,a));
    }
    rng=0xdeadbeef;
    for(int i=0;i<260;i++){int sx=rng_next()%fb.width,sy=rng_next()%dh;uint8_t br=60+(rng_next()%195);fb_putpixel(sx,sy,rgb(br,br,br+20));}
    for(int i=0;i<20;i++){
        int sx=rng_next()%fb.width,sy=rng_next()%dh;fb_putpixel(sx,sy,0xffffff);
        if(sx>0)fb_putpixel(sx-1,sy,0x666688);
        if((unsigned)(sx+1)<fb.width)fb_putpixel(sx+1,sy,0x666688);
        if(sy>0)fb_putpixel(sx,sy-1,0x666688);
        if(sy<dh-1)fb_putpixel(sx,sy+1,0x666688);
    }
}

static void draw_icons(void){
    if(!vfs.mounted)return;
    int mc=(fb.width-ICON_GRID_X*2)/ICON_CELL_W;if(mc<1)mc=1;
    int col=0,row=0,slot=0;
    for(int i=0;i<VFS_MAX_FILES;i++){
        if(!vfs.entries[i].used)continue;
        vfs_entry_t *e=&vfs.entries[i];
        int ix=ICON_GRID_X+col*ICON_CELL_W,iy=ICON_GRID_Y+row*ICON_CELL_H;
        if(i==desktop.selected_icon){fb_fill_rect(ix-2,iy-2,ICON_W+4,ICON_H+ICON_LABEL_H+4,0x2a4a7a);fb_draw_rect(ix-2,iy-2,ICON_W+4,ICON_H+ICON_LABEL_H+4,0x4a7fa5);}
        if(e->is_dir){
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
        char label[16];int li=0;
        while(e->name[li]&&li<9){label[li]=e->name[li];li++;}
        if(e->name[li]){label[li++]='.';label[li++]='.';}label[li]=0;
        int lw=text_strlen(label)*(8+1);
        text_puts(ix+(ICON_W-lw)/2,iy+ICON_H+2,label,0xd8e8f8,0,1);
        slot++;col=slot%mc;row=slot/mc;
    }
}

static void draw_taskbar(void){
    int ty=fb.height-TASKBAR_HEIGHT;
    fb_fill_gradient_v(0,ty,fb.width,TASKBAR_HEIGHT,0x0e1628,COL_TASKBAR_BG);
    fb_draw_hline(0,ty,fb.width,COL_TASKBAR_BORDER);
    fb_draw_hline(0,ty+1,fb.width,0x1a2a40);
    int bh=TASKBAR_HEIGHT-8,by=ty+4;
    fb_fill_gradient_v(6,by,56,bh,0x5a8fbf,0x3a6f9f);
    fb_draw_rect(6,by,56,bh,0x7aafdf);
    text_puts_centered(6,by+(bh-8)/2,56,"Velox",0xffffff,0,1);
    int wx=70;
    for(window_node_t *node=desktop.windows; node; node=node->next){
        window_t *w = node->win;
        if(!w->visible&&!w->minimized)continue;
        int active=(node==desktop.active_win);
        fb_fill_rect(wx,by,110,bh,active?COL_TASKBAR_BTN_ACT:COL_TASKBAR_BTN);
        fb_draw_rect(wx,by,110,bh,active?COL_TASKBAR_BORDER:0x2a3a5a);
        if(active)fb_fill_rect(wx+2,by+bh-3,106,2,COL_TASKBAR_BORDER);
        if(w->edit_dirty)fb_fill_rect(wx+4,by+bh/2-2,4,4,0xe0a030);
        else{uint32_t dot=w->minimized?0x888888:(active?0x80c0ff:0x405070);fb_fill_rect(wx+4,by+bh/2-2,4,4,dot);}
        text_puts(wx+14,by+(bh-8)/2,w->title,active?0xf0f0f0:0x8899bb,0,1);
        wx+=118;
    }
    int tw=90,tx=fb.width-tw-4;
    fb_fill_rect(tx,by,tw,bh,COL_TRAY_BG);
    fb_draw_rect(tx,by,tw,bh,0x2a3a5a);
    rtc_time_t t;rtc_read(&t);char tb[9];rtc_format_time(&t,tb);
    text_puts_centered(tx,by+(bh-8)/2,tw,tb,COL_CLOCK,0,1);
}

static void draw_cursor_at(int mx,int my){
    for(int r=0;r<CURSOR_H;r++)for(int c=0;c<CURSOR_W;c++){
        int px=mx+c,py=my+r;
        if(px<0||py<0||px>=(int)fb.width||py>=(int)fb.height)continue;
        if(cursor_bmp[r][c]==1)fb_putpixel(px,py,0x000000);
        else if(cursor_bmp[r][c]==2)fb_putpixel(px,py,0xffffff);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void desktop_init(void){
    desktop.windows=NULL;
    desktop.nwindows=0;
    desktop.active_win=NULL;
    desktop.mx=fb.width/2;desktop.my=fb.height/2;
    desktop.btn_left=0;desktop.btn_right=0;
    desktop.dirty=1;desktop.needs_full_redraw=1;
    desktop.cursor_saved=0;desktop.selected_icon=-1;
    menu_clear();
}

window_node_t *desktop_add_window(int x,int y,int w,int h,const char *title){
    // Allocate window
    window_t *win = (window_t *)mm_alloc(sizeof(window_t));
    if(!win) return NULL;
    
    // Allocate node
    window_node_t *node = (window_node_t *)mm_alloc(sizeof(window_node_t));
    if(!node){mm_free(win); return NULL;}
    
    // Initialize window
    win->x=x;win->y=y;win->w=w;win->h=h;
    win->visible=1;win->minimized=0;win->dragging=0;
    win->has_content=0;win->content[0]=0;
    win->editable=0;win->edit_dirty=0;win->edit_len=0;
    win->fs_idx=-1;win->scroll_row=0;
    int i=0;while(title[i]&&i<TITLE_MAX-1){win->title[i]=title[i];i++;}win->title[i]=0;
    
    // Append to list
    node->win = win;
    node->next = NULL;
    
    if(!desktop.windows){
        desktop.windows = node;
    } else {
        window_node_t *curr = desktop.windows;
        while(curr->next) curr = curr->next;
        curr->next = node;
    }
    
    desktop.nwindows++;
    desktop.active_win = node;
    desktop.needs_full_redraw=1;
    
    return node;
}

void desktop_redraw(void){
    draw_wallpaper();draw_icons();
    
    // Draw all windows
    for(window_node_t *node=desktop.windows; node; node=node->next)
        window_draw(node->win, node==desktop.active_win);
    
    draw_taskbar();menu_draw();input_draw();
    int sx=desktop.mx,sy=desktop.my;
    if(sx+CURSOR_W>(int)fb.width)sx=fb.width-CURSOR_W;
    if(sy+CURSOR_H>(int)fb.height)sy=fb.height-CURSOR_H;
    if(sx<0)sx=0;if(sy<0)sy=0;
    fb_save_region(sx,sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
    desktop.cursor_saved=1;desktop.cursor_sx=sx;desktop.cursor_sy=sy;
    draw_cursor_at(desktop.mx,desktop.my);
    fb_flip();desktop.dirty=0;desktop.needs_full_redraw=0;
}

void desktop_update_cursor(void){
    if(desktop.cursor_saved){
        fb_restore_region(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
        fb_flip_rect(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H);
    }
    int sx=desktop.mx,sy=desktop.my;
    if(sx+CURSOR_W>(int)fb.width)sx=fb.width-CURSOR_W;
    if(sy+CURSOR_H>(int)fb.height)sy=fb.height-CURSOR_H;
    if(sx<0)sx=0;if(sy<0)sy=0;
    fb_save_region(sx,sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
    desktop.cursor_sx=sx;desktop.cursor_sy=sy;desktop.cursor_saved=1;
    draw_cursor_at(desktop.mx,desktop.my);
    fb_flip_rect(sx,sy,CURSOR_W,CURSOR_H);
}

// ── Keyboard routing (called from kernel main loop) ───────────────────────────
void desktop_handle_key(uint8_t scancode){
    if(input_box.visible){input_handle_key(scancode);input_box.dirty=1;return;}
    if(desktop.active_win){
        if(window_handle_key(desktop.active_win->win,scancode))
            desktop.needs_full_redraw=1;
    }
}

void desktop_mouse_move(int dx,int dy,int btn_left,int btn_right){
    if(desktop.cursor_saved)
        fb_restore_region(desktop.cursor_sx,desktop.cursor_sy,CURSOR_W,CURSOR_H,desktop.cursor_save);
    desktop.mx+=dx;desktop.my+=dy;
    if(desktop.mx<0)desktop.mx=0;if(desktop.my<0)desktop.my=0;
    if(desktop.mx>=(int)fb.width)desktop.mx=fb.width-1;
    if(desktop.my>=(int)fb.height)desktop.my=fb.height-1;

    if(ctx_menu.visible){menu_handle_hover(desktop.mx,desktop.my);desktop.needs_full_redraw=1;}
    if(input_box.visible)desktop.needs_full_redraw=1;

    int lc=btn_left&&!desktop.btn_left;
    int rc=btn_right&&!desktop.btn_right;
    desktop.btn_left=btn_left;desktop.btn_right=btn_right;

    // Right click
    if(rc&&!input_box.visible){
        int icon=icon_at(desktop.mx,desktop.my);
        menu_clear();
        if(icon>=0){
            menu_icon_target=icon;desktop.selected_icon=icon;
            menu_add_item("Open",action_open);
            menu_add_separator();
            menu_add_item("Rename",action_rename);
            menu_add_item("Delete",action_delete);
        } else {
            menu_icon_target=-1;
            int ow=0;
            for(window_node_t *node=desktop.windows; node; node=node->next){
                window_t *w=node->win;
                if(!w->visible)continue;
                if(desktop.mx>=w->x&&desktop.mx<w->x+w->w&&desktop.my>=w->y&&desktop.my<w->y+w->h){ow=1;break;}
            }
            if(!ow){menu_add_item("New File",action_new_file);menu_add_item("New Folder",action_new_folder);menu_add_separator();menu_add_item("Refresh",action_refresh);}
        }
        if(ctx_menu.nitems>0)menu_show(desktop.mx,desktop.my);
        desktop.needs_full_redraw=1;
    }

    // Left click
    if(lc){
        if(input_box.visible){input_handle_click(desktop.mx,desktop.my);desktop.needs_full_redraw=1;goto done;}
        if(ctx_menu.visible){menu_handle_click(desktop.mx,desktop.my);desktop.needs_full_redraw=1;goto done;}

        // Window buttons (check all windows)
        window_node_t *topmost=NULL;
        for(window_node_t *node=desktop.windows; node; node=node->next){
            window_t *w=node->win;
            if(!w->visible)continue;
            int hit=window_hit_button(w,desktop.mx,desktop.my);
            if(hit){topmost=node; break;}
        }
        if(topmost){
            window_t *w=topmost->win;
            int hit=window_hit_button(w,desktop.mx,desktop.my);
            if(hit==BTN_CLOSE){try_close_window(topmost);goto done;}
            if(hit==BTN_MIN){w->minimized=!w->minimized;w->visible=!w->minimized;desktop.needs_full_redraw=1;goto done;}
        }

        // Window drag/focus (check all windows, first match wins)
        int cw=0;
        for(window_node_t *node=desktop.windows; node; node=node->next){
            window_t *w=node->win;
            if(!w->visible)continue;
            if(desktop.mx>=w->x&&desktop.mx<w->x+w->w&&desktop.my>=w->y&&desktop.my<w->y+w->h){
                desktop.active_win=node;cw=1;
                if(desktop.my<w->y+TITLEBAR_HEIGHT){w->dragging=1;w->drag_ox=desktop.mx-w->x;w->drag_oy=desktop.my-w->y;}
                desktop.needs_full_redraw=1;goto done;
            }
        }

        if(!cw){
            int icon=icon_at(desktop.mx,desktop.my);
            if(icon>=0){
                if(icon==desktop.selected_icon){open_entry(icon);desktop.selected_icon=-1;}
                else{desktop.selected_icon=icon;desktop.needs_full_redraw=1;}
            } else {desktop.selected_icon=-1;desktop.needs_full_redraw=1;}
        }
    }

done:
    if(!btn_left)for(window_node_t *node=desktop.windows; node; node=node->next)
        node->win->dragging=0;
    
    if(btn_left){
        for(window_node_t *node=desktop.windows; node; node=node->next){
            window_t *w=node->win;
            if(!w->dragging)continue;
            int nx=desktop.mx-w->drag_ox,ny=desktop.my-w->drag_oy;
            if(ny<0)ny=0;
            if(ny+w->h>(int)fb.height-TASKBAR_HEIGHT)ny=fb.height-TASKBAR_HEIGHT-w->h;
            if(nx!=w->x||ny!=w->y){w->x=nx;w->y=ny;desktop.needs_full_redraw=1;}
        }
    }
    desktop.dirty=1;
}