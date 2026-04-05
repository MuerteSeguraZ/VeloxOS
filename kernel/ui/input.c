#include "input.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

input_box_t input_box;

// ── Colors ────────────────────────────────────────────────────────────────────
#define COL_INPUT_BG      0x12121e
#define COL_INPUT_BORDER  0x4a7fa5
#define COL_INPUT_FIELD   0x0a0a18
#define COL_INPUT_TEXT    0xe0f0ff
#define COL_INPUT_CURSOR  0x80c0ff
#define COL_INPUT_TITLE   0xc0d8f0
#define COL_BTN_OK        0x2a6a3a
#define COL_BTN_CANCEL    0x6a2a2a
#define COL_BTN_BORDER    0x4a9a6a
#define COL_BTN_BORDER_C  0x9a4a4a
#define COL_SHADOW        0x000000

// Scancode → ASCII (US QWERTY, unshifted)
static const char sc_to_ascii[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', 0,  0,
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
};

// Scancode → ASCII (shifted)
static const char sc_to_ascii_shift[128] = {
    0,  0, '!','@','#','$','%','^','&','*','(',')','_','+', 0,  0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
};

static int shift_held = 0;

void input_show(const char *title, const char *initial,
                input_confirm_cb on_confirm,
                input_cancel_cb  on_cancel,
                void *userdata) {
    input_box.w = 320;
    input_box.h = 110;
    input_box.x = (fb.width  - input_box.w) / 2;
    input_box.y = (fb.height - input_box.h) / 2;

    int i = 0;
    while (title[i] && i < 31) { input_box.title[i] = title[i]; i++; }
    input_box.title[i] = 0;

    input_box.len = 0;
    if (initial) {
        while (initial[input_box.len] && input_box.len < INPUT_MAX-1) {
            input_box.buf[input_box.len] = initial[input_box.len];
            input_box.len++;
        }
    }
    input_box.buf[input_box.len] = 0;

    input_box.on_confirm  = on_confirm;
    input_box.on_cancel   = on_cancel;
    input_box.userdata    = userdata;
    input_box.visible     = 1;
    input_box.cursor_blink = 0;
    shift_held = 0;
}

void input_hide(void) {
    input_box.visible = 0;
}

int input_active(void) {
    return input_box.visible;
}

void input_draw(void) {
    if (!input_box.visible) return;

    int x = input_box.x, y = input_box.y;
    int w = input_box.w, h = input_box.h;

    // Shadow
    fb_fill_rect(x+5, y+5, w, h, COL_SHADOW);

    // Background + border
    fb_fill_rect(x, y, w, h, COL_INPUT_BG);
    fb_draw_rect(x, y, w, h, COL_INPUT_BORDER);
    fb_draw_hline(x+1, y+1, w-2, 0x2a4a6a);

    // Title bar
    fb_fill_gradient_v(x+1, y+1, w-2, 18, 0x1e3a5f, 0x0d2137);
    fb_draw_hline(x, y+19, w, COL_INPUT_BORDER);
    text_puts_centered(x, y+5, w, input_box.title, COL_INPUT_TITLE, 0, 1);

    // Prompt
    text_puts(x+12, y+28, "Name:", 0x8899bb, 0, 1);

    // Input field
    int fx = x+12, fy = y+40, fw = w-24, fh = 18;
    fb_fill_rect(fx, fy, fw, fh, COL_INPUT_FIELD);
    fb_draw_rect(fx, fy, fw, fh, COL_INPUT_BORDER);

    // Text in field
    text_puts(fx+4, fy+4, input_box.buf, COL_INPUT_TEXT, 0, 1);

    // Blinking cursor
    input_box.cursor_blink++;
    if ((input_box.cursor_blink / 30) % 2 == 0) {
        int cx = fx + 4 + input_box.len * (GLYPH_W+1);
        fb_fill_rect(cx, fy+3, 2, fh-6, COL_INPUT_CURSOR);
    }

    // OK button
    int btn_y = y + h - 28;
    fb_fill_rect(x+12, btn_y, 80, 20, COL_BTN_OK);
    fb_draw_rect(x+12, btn_y, 80, 20, COL_BTN_BORDER);
    text_puts_centered(x+12, btn_y+6, 80, "OK", 0xffffff, 0, 1);

    // Cancel button
    fb_fill_rect(x+w-92, btn_y, 80, 20, COL_BTN_CANCEL);
    fb_draw_rect(x+w-92, btn_y, 80, 20, COL_BTN_BORDER_C);
    text_puts_centered(x+w-92, btn_y+6, 80, "Cancel", 0xffffff, 0, 1);
}

void input_handle_key(uint8_t scancode) {
    if (!input_box.visible) return;

    // Track shift
    if (scancode == 0x2A || scancode == 0x36) { shift_held = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_held = 0; return; }

    // Key release (bit 7 set) — ignore except shift above
    if (scancode & 0x80) return;

    // Enter = confirm
    if (scancode == 0x1C) {
        input_box.visible = 0;
        if (input_box.on_confirm)
            input_box.on_confirm(input_box.buf, input_box.userdata);
        return;
    }

    // Escape = cancel
    if (scancode == 0x01) {
        input_box.visible = 0;
        if (input_box.on_cancel)
            input_box.on_cancel(input_box.userdata);
        return;
    }

    // Backspace
    if (scancode == 0x0E) {
        if (input_box.len > 0)
            input_box.buf[--input_box.len] = 0;
        return;
    }

    // Printable character
    if (scancode < 128) {
        char c = shift_held ? sc_to_ascii_shift[scancode]
                            : sc_to_ascii[scancode];
        if (c && input_box.len < INPUT_MAX-1) {
            input_box.buf[input_box.len++] = c;
            input_box.buf[input_box.len]   = 0;
        }
    }
}