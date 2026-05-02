#pragma once
#include "../../stdint.h"

// ── Key codes ─────────────────────────────────────────────────────────────────
#define KEY_NONE        0x00
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0D
#define KEY_ESCAPE      0x1B
#define KEY_DELETE      0x7F

#define KEY_SHIFT_L     0x80
#define KEY_SHIFT_R     0x81
#define KEY_CTRL_L      0x82
#define KEY_CTRL_R      0x83
#define KEY_ALT_L       0x84
#define KEY_ALT_R       0x85
#define KEY_CAPS_LOCK   0x86
#define KEY_F1          0x90
#define KEY_F2          0x91
#define KEY_F3          0x92
#define KEY_F4          0x93
#define KEY_F5          0x94
#define KEY_F6          0x95
#define KEY_F7          0x96
#define KEY_F8          0x97
#define KEY_F9          0x98
#define KEY_F10         0x99
#define KEY_F11         0x9A
#define KEY_F12         0x9B
#define KEY_UP          0xA0
#define KEY_DOWN        0xA1
#define KEY_LEFT        0xA2
#define KEY_RIGHT       0xA3
#define KEY_HOME        0xA4
#define KEY_END         0xA5
#define KEY_PAGE_UP     0xA6
#define KEY_PAGE_DOWN   0xA7

// ── Modifier flags ────────────────────────────────────────────────────────────
#define MOD_SHIFT   (1 << 0)
#define MOD_CTRL    (1 << 1)
#define MOD_ALT     (1 << 2)
#define MOD_CAPS    (1 << 3)

// ── Ring buffer ───────────────────────────────────────────────────────────────
#define KBD_BUF_SIZE 64

// ── Key event ─────────────────────────────────────────────────────────────────
typedef struct {
    uint8_t  scancode;   // KEY_* value or ASCII char (named scancode for compat)
    uint8_t  mods;       // MOD_* flags
    uint8_t  pressed;    // 1 = key down, 0 = key up
    char     ascii;      // final printable char (0 if non-printable)
} key_event_t;

// ── API ───────────────────────────────────────────────────────────────────────
void    kbd_init(void);
void    kbd_enqueue_scancode(uint8_t raw_scancode);
int     kbd_poll(key_event_t *evt);
uint8_t kbd_get_mods(void);