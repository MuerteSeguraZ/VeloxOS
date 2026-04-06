#include "window.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"
#include "../drivers/keyboard/keyboard.h"

#define COL_WIN_TITLEBAR_ACT   0x1e3a5f
#define COL_WIN_TITLEBAR_INACT 0x1a1a2e
#define COL_WIN_TITLEBAR_GRAD  0x0d2137
#define COL_WIN_BODY           0x0e0e1a
#define COL_WIN_BODY2          0x181828
#define COL_WIN_BORDER_ACT     0x4a7fa5
#define COL_WIN_BORDER_INACT   0x2a2a4a
#define COL_TEXT_WHITE         0xf0f0f0
#define COL_TEXT_DIM           0x7080a0
#define COL_TEXT_CONTENT       0xc8d8e8
#define COL_TEXT_CURSOR        0x80c0ff
#define COL_BTN_CLOSE          0xe05050
#define COL_BTN_MIN            0xe0a030
#define COL_BTN_MAX            0x40b060
#define COL_DIRTY_DOT          0xe0a030

static void mem_cpy(char *d,const char *s,int n){while(n--)*d++=*s++;}

void window_set_content(window_t *win,const char *text,uint32_t len){
    if(len>=WIN_CONTENT_MAX)len=WIN_CONTENT_MAX-1;
    mem_cpy(win->content,text,len); win->content[len]=0;
    win->has_content=1; win->editable=0; win->edit_dirty=0; win->fs_idx=-1;
}

void window_set_editable(window_t *win,const char *text,uint32_t len,int fs_idx){
    if(len>=WIN_CONTENT_MAX)len=WIN_CONTENT_MAX-1;
    mem_cpy(win->edit_buf,text,len); win->edit_buf[len]=0;
    mem_cpy(win->orig_buf,text,len); win->orig_buf[len]=0;
    mem_cpy(win->content, text,len); win->content[len]=0;
    win->edit_len=len; win->has_content=1;
    win->editable=1; win->edit_dirty=0;
    win->fs_idx=fs_idx; win->scroll_row=0;
}

// Takes a key_event_t — no scancode tables here
int window_handle_key(window_t *win, const key_event_t *evt){
    if(!win->editable||!win->visible||!evt->pressed)return 0;

    // Backspace
    if(evt->scancode==KEY_BACKSPACE){
        if(win->edit_len>0){win->edit_buf[--win->edit_len]=0;win->edit_dirty=1;}
        return 1;
    }

    // Escape — deselect / unfocus (caller handles)
    if(evt->scancode==KEY_ESCAPE)return 1;

    // Printable + Enter (\n)
    if(evt->ascii&&win->edit_len<WIN_CONTENT_MAX-1){
        win->edit_buf[win->edit_len++]=evt->ascii;
        win->edit_buf[win->edit_len]=0;
        win->edit_dirty=1;

        // Auto-scroll
        int max_cols=(win->w-24)/(8+1); if(max_cols<1)max_cols=1;
        int max_rows=(win->h-TITLEBAR_HEIGHT-24)/(8+3); if(max_rows<1)max_rows=1;
        int row=0,col=0;
        for(int i=0;i<win->edit_len;i++){
            if(win->edit_buf[i]=='\n'||col>=max_cols){row++;col=0;}
            else col++;
        }
        if(row-win->scroll_row>=max_rows)win->scroll_row=row-max_rows+1;
        return 1;
    }
    return 0;
}

void window_draw(window_t *win,int active){
    if(!win->visible)return;
    int x=win->x,y=win->y,w=win->w,h=win->h;
    uint32_t border=active?COL_WIN_BORDER_ACT:COL_WIN_BORDER_INACT;

    fb_fill_rect(x+5,y+5,w,h,0x000000);
    fb_fill_rect(x-WINDOW_BORDER,y-WINDOW_BORDER,
                 w+WINDOW_BORDER*2,h+WINDOW_BORDER*2,border);

    if(active)fb_fill_gradient_v(x,y,w,TITLEBAR_HEIGHT,COL_WIN_TITLEBAR_ACT,COL_WIN_TITLEBAR_GRAD);
    else fb_fill_rect(x,y,w,TITLEBAR_HEIGHT,COL_WIN_TITLEBAR_INACT);
    fb_draw_hline(x,y+TITLEBAR_HEIGHT-1,w,border);
    fb_fill_gradient_v(x,y+TITLEBAR_HEIGHT,w,h-TITLEBAR_HEIGHT,COL_WIN_BODY,COL_WIN_BODY2);

    int by=y+TITLEBAR_HEIGHT/2-5;
    fb_fill_rect(x+8, by,10,10,active?COL_BTN_CLOSE:0x444444); fb_draw_rect(x+8, by,10,10,0x00000030);
    fb_fill_rect(x+22,by,10,10,active?COL_BTN_MIN:0x444444);   fb_draw_rect(x+22,by,10,10,0x00000030);
    fb_fill_rect(x+36,by,10,10,active?COL_BTN_MAX:0x444444);   fb_draw_rect(x+36,by,10,10,0x00000030);
    if(win->edit_dirty)fb_fill_rect(x+11,by+3,4,4,COL_DIRTY_DOT);

    int title_px=text_strlen(win->title)*(8+1);
    int title_x=x+(w-title_px)/2, title_y=y+(TITLEBAR_HEIGHT-8)/2;
    text_puts(title_x,title_y,win->title,active?COL_TEXT_WHITE:COL_TEXT_DIM,0,1);
    if(win->editable&&active){
        const char *tag=win->edit_dirty?" [+]":" [~]";
        text_puts(title_x+title_px+2,title_y,tag,0x8899bb,0,1);
    }

    int cx=x+12,cy=y+TITLEBAR_HEIGHT+10;
    int max_w=w-24,max_h=h-TITLEBAR_HEIGHT-20;
    int max_cols=max_w/(8+1); if(max_cols<1)max_cols=1;
    int max_rows=max_h/(8+3); if(max_rows<1)max_rows=1;
    const char *src=win->editable?win->edit_buf:win->content;

    if(win->has_content||win->editable){
        fb_fill_rect(x+w-8,y+TITLEBAR_HEIGHT,6,h-TITLEBAR_HEIGHT,0x0a0a14);

        // Count total rows for scrollbar
        const char *pp=src; int tot=0,tc=0;
        while(*pp){if(*pp=='\n'||tc>=max_cols){tot++;tc=0;}else tc++;pp++;}tot++;
        if(tot>max_rows){
            int th=max_rows*max_rows/tot; if(th<4)th=4;
            int ty2=(h-TITLEBAR_HEIGHT)*win->scroll_row/tot;
            fb_fill_rect(x+w-7,y+TITLEBAR_HEIGHT+ty2,4,th,0x4a7fa5);
        }

        // Render
        const char *p=src; int row=0,col=0;
        while(*p){
            if(*p=='\n'){row++;col=0;p++;if(row-win->scroll_row>=max_rows)break;continue;}
            if(col>=max_cols){row++;col=0;if(row-win->scroll_row>=max_rows)break;}
            int dr=row-win->scroll_row;
            if(dr>=0)text_putchar(cx+col*(8+1),cy+dr*(8+3),*p,COL_TEXT_CONTENT,0,1);
            col++;p++;
        }

        // Cursor
        if(win->editable&&active){
            const char *ep=win->edit_buf; int cr=0,cc=0;
            while(*ep){if(*ep=='\n'||cc>=max_cols){cr++;cc=0;}else cc++;ep++;}
            int dr=cr-win->scroll_row;
            if(dr>=0&&dr<max_rows)fb_fill_rect(cx+cc*(8+1),cy+dr*(8+3),2,8,COL_TEXT_CURSOR);
        }
    } else {
        text_puts(cx,cy,win->title,0x6090c0,0,1);
        text_puts(cx,cy+16,"Velox OS v0.1",COL_TEXT_DIM,0,1);
        fb_draw_hline(cx,cy+30,max_w,0x2a3a5a);
        text_puts(cx,cy+38,"Window compositor active.",COL_TEXT_DIM,0,1);
        text_puts(cx,cy+54,"PS/2 mouse + keyboard.",COL_TEXT_DIM,0,1);
        text_puts(cx,cy+70,"PIT timer @ 60hz.",COL_TEXT_DIM,0,1);
    }
}

int window_hit_button(window_t *win,int mx,int my){
    if(!win->visible)return 0;
    int by=win->y+TITLEBAR_HEIGHT/2-5;
    if(my<by||my>=by+10)return 0;
    if(mx>=win->x+8 &&mx<win->x+18)return BTN_CLOSE;
    if(mx>=win->x+22&&mx<win->x+32)return BTN_MIN;
    if(mx>=win->x+36&&mx<win->x+46)return BTN_MAX;
    return 0;
}