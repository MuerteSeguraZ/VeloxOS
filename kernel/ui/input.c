#include "input.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

input_box_t input_box;

#define COL_BG        0x12121e
#define COL_BORDER    0x4a7fa5
#define COL_FIELD     0x0a0a18
#define COL_TEXT      0xe0f0ff
#define COL_CURSOR    0x80c0ff
#define COL_TITLE_FG  0xc0d8f0
#define COL_MSG       0xa0b8d0
#define COL_BTN_YES   0x2a6a3a
#define COL_BTN_NO    0x6a2a2a
#define COL_BTN_CANC  0x2a2a5a
#define COL_BD_YES    0x4a9a6a
#define COL_BD_NO     0x9a4a4a
#define COL_BD_CANC   0x4a4a9a

static const char sc_ascii[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};
static const char sc_ascii_sh[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};
static int shift=0;

static void str_cpy(char *d, const char *s, int max) {
    int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]=0;
}

// ── Common setup ──────────────────────────────────────────────────────────────
static void setup_box(int ync) {
    input_box.w = ync ? 300 : 320;
    input_box.h = ync ? 110 : 110;
    input_box.x = (fb.width  - input_box.w) / 2;
    input_box.y = (fb.height - input_box.h) / 2;
    input_box.visible      = 1;
    input_box.cursor_blink = 0;
    input_box.dirty        = 1;
    shift = 0;
}

void input_show(const char *title, const char *initial,
                input_confirm_cb on_confirm,
                input_cancel_cb  on_cancel,
                void *userdata) {
    input_box.mode = INPUT_MODE_TEXT;
    setup_box(0);
    str_cpy(input_box.title, title, 48);
    input_box.message[0] = 0;
    input_box.len = 0;
    if (initial) {
        while(initial[input_box.len]&&input_box.len<INPUT_MAX-1) {
            input_box.buf[input_box.len]=initial[input_box.len];
            input_box.len++;
        }
    }
    input_box.buf[input_box.len]=0;
    input_box.on_confirm = on_confirm;
    input_box.on_no      = 0;
    input_box.on_cancel  = on_cancel;
    input_box.userdata   = userdata;
}

void input_show_ync(const char *title, const char *message,
                    input_confirm_cb on_yes,
                    input_no_cb      on_no,
                    input_cancel_cb  on_cancel,
                    void *userdata) {
    input_box.mode = INPUT_MODE_YNC;
    setup_box(1);
    str_cpy(input_box.title,   title,   48);
    str_cpy(input_box.message, message, 128);
    input_box.buf[0]     = 0;
    input_box.len        = 0;
    input_box.on_confirm = on_yes;
    input_box.on_no      = on_no;
    input_box.on_cancel  = on_cancel;
    input_box.userdata   = userdata;
}

void input_hide(void) { input_box.visible=0; input_box.dirty=0; }
int  input_active(void) { return input_box.visible; }

// ── Button geometry ───────────────────────────────────────────────────────────
// TEXT mode: [OK] [Cancel]
// YNC  mode: [Yes] [No] [Cancel]
static int btn_row_y(void) { return input_box.y + input_box.h - 30; }

// TEXT mode buttons
static int tok_x(void)   { return input_box.x + 12; }
static int tcanc_x(void) { return input_box.x + input_box.w - 92; }

// YNC mode buttons  (3 buttons spread across width)
static int yyes_x(void)  { return input_box.x + 12; }
static int yno_x(void)   { return input_box.x + input_box.w/2 - 36; }
static int ycanc_x(void) { return input_box.x + input_box.w - 84; }

static void draw_btn(int x, int y, int w, int h,
                     const char *label,
                     uint32_t bg, uint32_t bd) {
    fb_fill_rect(x,y,w,h,bg);
    fb_draw_rect(x,y,w,h,bd);
    text_puts_centered(x,y+(h-8)/2,w,label,0xffffff,0,1);
}

void input_draw(void) {
    if (!input_box.visible) return;
    int x=input_box.x, y=input_box.y, w=input_box.w, h=input_box.h;

    // Shadow
    fb_fill_rect(x+5,y+5,w,h,0x000000);
    // Background
    fb_fill_rect(x,y,w,h,COL_BG);
    fb_draw_rect(x,y,w,h,COL_BORDER);
    fb_draw_hline(x+1,y+1,w-2,0x2a4a6a);
    // Titlebar
    fb_fill_gradient_v(x+1,y+1,w-2,18,0x1e3a5f,0x0d2137);
    fb_draw_hline(x,y+19,w,COL_BORDER);
    text_puts_centered(x,y+5,w,input_box.title,COL_TITLE_FG,0,1);

    int by=btn_row_y();

    if (input_box.mode == INPUT_MODE_TEXT) {
        // Prompt + field
        text_puts(x+12,y+28,"Name:",0x8899bb,0,1);
        int fx=x+12,fy=y+42,fw=w-24,fh=18;
        fb_fill_rect(fx,fy,fw,fh,COL_FIELD);
        fb_draw_rect(fx,fy,fw,fh,COL_BORDER);
        text_puts(fx+4,fy+4,input_box.buf,COL_TEXT,0,1);
        // Cursor blink
        input_box.cursor_blink++;
        if ((input_box.cursor_blink/30)%2==0) {
            int cx=fx+4+input_box.len*(8+1);
            fb_fill_rect(cx,fy+3,2,fh-6,COL_CURSOR);
        }
        // Buttons
        draw_btn(tok_x(),   by, 80,20,"OK",    COL_BTN_YES, COL_BD_YES);
        draw_btn(tcanc_x(), by, 80,20,"Cancel",COL_BTN_NO,  COL_BD_NO);

    } else {
        // YNC — show message
        text_puts_centered(x,y+38,w,input_box.message,COL_MSG,0,1);
        // Divider
        fb_draw_hline(x+12,y+54,w-24,0x2a3a5a);
        // Buttons
        draw_btn(yyes_x(),  by, 70,20,"Yes",   COL_BTN_YES, COL_BD_YES);
        draw_btn(yno_x(),   by, 70,20,"No",    COL_BTN_NO,  COL_BD_NO);
        draw_btn(ycanc_x(), by, 70,20,"Cancel",COL_BTN_CANC,COL_BD_CANC);
    }
}

int input_handle_click(int mx, int my) {
    if (!input_box.visible) return 0;
    int by=btn_row_y();

    if (input_box.mode == INPUT_MODE_TEXT) {
        if (mx>=tok_x()&&mx<tok_x()+80&&my>=by&&my<by+20) {
            input_box.visible=0;
            if (input_box.on_confirm)
                input_box.on_confirm(input_box.buf,input_box.userdata);
            return 1;
        }
        if (mx>=tcanc_x()&&mx<tcanc_x()+80&&my>=by&&my<by+20) {
            input_box.visible=0;
            if (input_box.on_cancel)
                input_box.on_cancel(input_box.userdata);
            return 1;
        }
    } else {
        if (mx>=yyes_x()&&mx<yyes_x()+70&&my>=by&&my<by+20) {
            input_box.visible=0;
            if (input_box.on_confirm)
                input_box.on_confirm(0,input_box.userdata);
            return 1;
        }
        if (mx>=yno_x()&&mx<yno_x()+70&&my>=by&&my<by+20) {
            input_box.visible=0;
            if (input_box.on_no)
                input_box.on_no(input_box.userdata);
            return 1;
        }
        if (mx>=ycanc_x()&&mx<ycanc_x()+70&&my>=by&&my<by+20) {
            input_box.visible=0;
            if (input_box.on_cancel)
                input_box.on_cancel(input_box.userdata);
            return 1;
        }
    }

    // Click inside box — consume
    if (mx>=input_box.x&&mx<input_box.x+input_box.w&&
        my>=input_box.y&&my<input_box.y+input_box.h)
        return 1;
    return 0;
}

void input_handle_key(uint8_t scancode) {
    if (!input_box.visible) return;

    if (scancode==0x2A||scancode==0x36) { shift=1; return; }
    if (scancode==0xAA||scancode==0xB6) { shift=0; return; }
    if (scancode & 0x80) return;

    if (input_box.mode == INPUT_MODE_YNC) {
        // Y = yes, N = no, Escape = cancel
        if (scancode==0x15) { // Y
            input_box.visible=0;
            if(input_box.on_confirm) input_box.on_confirm(0,input_box.userdata);
            input_box.dirty=1; return;
        }
        if (scancode==0x31) { // N
            input_box.visible=0;
            if(input_box.on_no) input_box.on_no(input_box.userdata);
            input_box.dirty=1; return;
        }
        if (scancode==0x01) { // Esc
            input_box.visible=0;
            if(input_box.on_cancel) input_box.on_cancel(input_box.userdata);
            input_box.dirty=1; return;
        }
        return;
    }

    // TEXT mode
    if (scancode==0x1C) { // Enter
        input_box.visible=0;
        if(input_box.on_confirm) input_box.on_confirm(input_box.buf,input_box.userdata);
        input_box.dirty=1; return;
    }
    if (scancode==0x01) { // Esc
        input_box.visible=0;
        if(input_box.on_cancel) input_box.on_cancel(input_box.userdata);
        input_box.dirty=1; return;
    }
    if (scancode==0x0E) { // Backspace
        if(input_box.len>0) input_box.buf[--input_box.len]=0;
        input_box.dirty=1; return;
    }
    if (scancode<128) {
        char c=shift?sc_ascii_sh[scancode]:sc_ascii[scancode];
        if(c&&input_box.len<INPUT_MAX-1) {
            input_box.buf[input_box.len++]=c;
            input_box.buf[input_box.len]=0;
        }
        input_box.dirty=1;
    }
}