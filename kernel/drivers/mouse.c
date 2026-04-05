#include "mouse.h"
#include "../arch/idt.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r;
}
static void ps2_wait_in(void)  { int t=100000; while(t-- && ( inb(PS2_STATUS)&0x02)); }
static void ps2_wait_out(void) { int t=100000; while(t-- && !(inb(PS2_STATUS)&0x01)); }

static uint8_t  pkt[3];
static int      pkt_byte = 0;

static volatile int acc_dx    = 0;
static volatile int acc_dy    = 0;
static volatile int acc_btn_l = 0;
static volatile int acc_btn_r = 0;
static volatile int has_data  = 0;

static void mouse_irq_handler(void) {
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 0x01)) return;

    uint8_t byte = inb(PS2_DATA);

    if (!(status & 0x20)) {
        pkt_byte = 0;
        return;
    }

    if (pkt_byte == 0 && !(byte & 0x08)) return;

    pkt[pkt_byte++] = byte;

    if (pkt_byte == 3) {
        pkt_byte = 0;

        int dx =  (int)pkt[1] - ((pkt[0] & 0x10) ? 256 : 0);
        int dy = -((int)pkt[2] - ((pkt[0] & 0x20) ? 256 : 0));

        if (dx < -127) dx = -127;
        if (dx >  127) dx =  127;
        if (dy < -127) dy = -127;
        if (dy >  127) dy =  127;

        acc_dx    += dx;
        acc_dy    += dy;
        acc_btn_l  = (pkt[0] & 0x01) ? 1 : 0;
        acc_btn_r  = (pkt[0] & 0x02) ? 1 : 0;
        has_data   = 1;
    }
}

void mouse_init(void) {
    ps2_wait_in();
    outb(PS2_CMD, 0xA8);

    ps2_wait_in();
    outb(PS2_CMD, 0x20);
    ps2_wait_out();
    uint8_t cb = inb(PS2_DATA) | 0x02;
    ps2_wait_in();
    outb(PS2_CMD, 0x60);
    ps2_wait_in();
    outb(PS2_DATA, cb);

    ps2_wait_in(); outb(PS2_CMD, 0xD4); ps2_wait_in(); outb(PS2_DATA, 0xF6);
    ps2_wait_out(); inb(PS2_DATA);

    ps2_wait_in(); outb(PS2_CMD, 0xD4); ps2_wait_in(); outb(PS2_DATA, 0xF4);
    ps2_wait_out(); inb(PS2_DATA);

    uint8_t mask = inb(0xA1) & ~(1 << 4);
    outb(0xA1, mask);
    mask = inb(0x21) & ~(1 << 2);
    outb(0x21, mask);

    irq_register(12, mouse_irq_handler);
}

int mouse_get_delta(int *dx, int *dy, int *btn_left, int *btn_right) {
    if (!has_data) return 0;
    __asm__ volatile ("cli");
    *dx        = acc_dx;
    *dy        = acc_dy;
    *btn_left  = acc_btn_l;
    *btn_right = acc_btn_r;
    acc_dx     = 0;
    acc_dy     = 0;
    has_data   = 0;
    __asm__ volatile ("sti");
    return 1;
}