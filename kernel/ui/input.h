#pragma once
#include "../stdint.h"
#include "../drivers/keyboard/keyboard.h"

#define INPUT_MAX 48

#define INPUT_MODE_TEXT   0
#define INPUT_MODE_YNC    1

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

typedef void (*input_confirm_cb)(const char *text, void *userdata);
typedef void (*input_cancel_cb)(void *userdata);
typedef void (*input_no_cb)(void *userdata);

typedef struct {
    int  visible;
    int  mode;
    char title[48];
    char message[128];
    char buf[INPUT_MAX];
    int  len;
    int  x, y, w, h;
    int  cursor_blink;
    int  dirty;

    input_confirm_cb on_confirm;
    input_no_cb      on_no;
    input_cancel_cb  on_cancel;
    void            *userdata;
} input_box_t;

extern input_box_t input_box;

void input_show(const char *title, const char *initial,
                input_confirm_cb on_confirm,
                input_cancel_cb  on_cancel,
                void *userdata);

void input_show_ync(const char *title, const char *message,
                    input_confirm_cb on_yes,
                    input_no_cb      on_no,
                    input_cancel_cb  on_cancel,
                    void *userdata);

void input_hide(void);
void input_draw(void);
void input_handle_key(const key_event_t *evt);
int  input_handle_click(int mx, int my);
int  input_active(void);