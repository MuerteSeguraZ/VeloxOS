#include "keyboard.h"

static key_event_t kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static void buf_push(key_event_t evt) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) return;  // full — drop oldest? no, drop newest
    kbd_buf[kbd_head] = evt;
    kbd_head = next;
}

// ── Modifier state ────────────────────────────────────────────────────────────
static uint8_t mods   = 0;
static int     caps_on = 0;

uint8_t kbd_get_mods(void) { return mods; }

// ── US QWERTY scancode set 1 → keycode ───────────────────────────────────────
static const uint8_t sc_to_key[128] = {
//  0             1              2    3    4    5    6    7
    0,            KEY_ESCAPE,   '1', '2', '3', '4', '5', '6',
//  8             9              A    B    C    D    E    F
    '7',          '8',          '9', '0', '-', '=', KEY_BACKSPACE, KEY_TAB,
//  10            11             12   13   14   15   16   17
    'q',          'w',          'e', 'r', 't', 'y', 'u', 'i',
//  18            19             1A   1B   1C   1D   1E   1F
    'o',          'p',          '[', ']', KEY_ENTER, KEY_CTRL_L, 'a', 's',
//  20            21             22   23   24   25   26   27
    'd',          'f',          'g', 'h', 'j', 'k', 'l', ';',
//  28            29             2A   2B   2C   2D   2E   2F
    '\'',         '`',          KEY_SHIFT_L, '\\', 'z', 'x', 'c', 'v',
//  30            31             32   33   34   35   36   37
    'b',          'n',          'm', ',', '.', '/', KEY_SHIFT_R, '*',
//  38            39             3A   3B   3C   3D   3E   3F
    KEY_ALT_L,    ' ',          KEY_CAPS_LOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
//  40            41             42   43   44   45   46   47
    KEY_F6,       KEY_F7,       KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
//  48            49             4A   4B   4C   4D   4E   4F
    KEY_UP,       KEY_PAGE_UP,  '-', KEY_LEFT, 0, KEY_RIGHT, '+', KEY_END,
//  50            51             52   53
    KEY_DOWN,     KEY_PAGE_DOWN, KEY_DELETE, 0,
    // rest zeroed
};

// Shifted printables
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

    // ── Update modifiers ──────────────────────────────────────────────────────
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

    // ── Build ASCII ───────────────────────────────────────────────────────────
    char ascii = 0;
    int shift  = (mods & MOD_SHIFT) != 0;

    if (keycode >= 32 && keycode < 127) {
        if (shift && sc_shifted[raw]) {
            ascii = sc_shifted[raw];
        } else {
            ascii = (char)keycode;
            // Apply caps lock to alpha only
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