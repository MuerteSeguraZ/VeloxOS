#pragma once
#include "../stdint.h"

#define INPUT_MAX 48

// Dialog modes
#define INPUT_MODE_TEXT   0   // text entry with OK/Cancel
#define INPUT_MODE_YNC    1   // Yes / No / Cancel (no text field)

typedef void (*input_confirm_cb)(const char *text, void *userdata);
typedef void (*input_cancel_cb)(void *userdata);
typedef void (*input_no_cb)(void *userdata);

typedef struct {
    int  visible;
    int  mode;              // INPUT_MODE_TEXT or INPUT_MODE_YNC
    char title[48];
    char message[128];      // shown in YNC mode instead of text field
    char buf[INPUT_MAX];
    int  len;
    int  x, y, w, h;
    int  cursor_blink;
    int  dirty;

    input_confirm_cb on_confirm;  // OK / Yes
    input_no_cb      on_no;       // No  (YNC only)
    input_cancel_cb  on_cancel;   // Cancel

    void *userdata;
} input_box_t;

extern input_box_t input_box;

// Text entry dialog
void input_show(const char *title, const char *initial,
                input_confirm_cb on_confirm,
                input_cancel_cb  on_cancel,
                void *userdata);

// Yes/No/Cancel dialog
void input_show_ync(const char *title, const char *message,
                    input_confirm_cb on_yes,
                    input_no_cb      on_no,
                    input_cancel_cb  on_cancel,
                    void *userdata);

void input_hide(void);
void input_draw(void);
void input_handle_key(uint8_t scancode);
int  input_handle_click(int mx, int my);
int  input_active(void);