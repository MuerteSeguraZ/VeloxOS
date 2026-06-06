#include "keyboard.h"

static key_event_t kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static void buf_push(key_event_t evt) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) return;
    kbd_buf[kbd_head] = evt;
    kbd_head = next;
}

static uint8_t mods   = 0;
static int     caps_on = 0;

uint8_t kbd_get_mods(void) { return mods; }

static const uint8_t sc_to_key[128] = {
    0,            KEY_ESCAPE,   '1', '2', '3', '4', '5', '6',
    '7',          '8',          '9', '0', '-', '=', KEY_BACKSPACE, KEY_TAB,
    'q',          'w',          'e', 'r', 't', 'y', 'u', 'i',
    'o',          'p',          '[', ']', KEY_ENTER, KEY_CTRL_L, 'a', 's',
    'd',          'f',          'g', 'h', 'j', 'k', 'l', ';',
    '\'',         '`',          KEY_SHIFT_L, '\\', 'z', 'x', 'c', 'v',
    'b',          'n',          'm', ',', '.', '/', KEY_SHIFT_R, '*',
    KEY_ALT_L,    ' ',          KEY_CAPS_LOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6,       KEY_F7,       KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
    KEY_UP,       KEY_PAGE_UP,  '-', KEY_LEFT, 0, KEY_RIGHT, '+', KEY_END,
    KEY_DOWN,     KEY_PAGE_DOWN, KEY_DELETE, 0,
};

static const char sc_shifted[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,0,0,' ',
};

void kbd_init(void) {
    kbd_head = 0;
    kbd_tail = 0;
    mods     = 0;
    caps_on  = 0;
}

void kbd_enqueue_scancode(uint8_t sc) {
    int released = (sc & 0x80) != 0;
    uint8_t raw  = sc & 0x7F;
    if (raw >= 128) return;

    uint8_t keycode = sc_to_key[raw];
    if (!keycode) return;

    switch (keycode) {
        case KEY_SHIFT_L: case KEY_SHIFT_R:
            if (released) mods &= ~MOD_SHIFT; else mods |= MOD_SHIFT;
            return;
        case KEY_CTRL_L: case KEY_CTRL_R:
            if (released) mods &= ~MOD_CTRL;  else mods |= MOD_CTRL;
            return;
        case KEY_ALT_L: case KEY_ALT_R:
            if (released) mods &= ~MOD_ALT;   else mods |= MOD_ALT;
            return;
        case KEY_CAPS_LOCK:
            if (!released) {
                caps_on = !caps_on;
                if (caps_on) mods |= MOD_CAPS; else mods &= ~MOD_CAPS;
            }
            return;
    }

    char ascii = 0;
    int shift  = (mods & MOD_SHIFT) != 0;

    if (keycode >= 32 && keycode < 127) {
        if (shift && sc_shifted[raw]) {
            ascii = sc_shifted[raw];
        } else {
            ascii = (char)keycode;
            if (caps_on && ascii >= 'a' && ascii <= 'z')
                ascii = ascii - 32;
        }
    }

    key_event_t evt;
    evt.scancode = keycode;
    evt.mods     = mods;
    evt.pressed  = released ? 0 : 1;
    evt.ascii    = ascii;
    buf_push(evt);
}

int kbd_poll(key_event_t *evt) {
    if (kbd_tail == kbd_head) return 0;
    *evt     = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return 1;
}