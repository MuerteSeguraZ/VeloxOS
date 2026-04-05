#pragma once
#include "../stdint.h"

#define INPUT_MAX 48

typedef void (*input_confirm_cb)(const char *text, void *userdata);
typedef void (*input_cancel_cb)(void *userdata);

typedef struct {
    int  visible;
    char title[32];
    char buf[INPUT_MAX];
    int  len;
    int  x, y, w, h;
    int  cursor_blink;   // tick counter for cursor blink

    input_confirm_cb on_confirm;
    input_cancel_cb  on_cancel;
    void            *userdata;
} input_box_t;

extern input_box_t input_box;

void input_show(const char *title, const char *initial,
                input_confirm_cb on_confirm,
                input_cancel_cb  on_cancel,
                void *userdata);
void input_hide(void);
void input_draw(void);
void input_handle_key(uint8_t scancode);  // raw PS/2 scancode
int  input_active(void);