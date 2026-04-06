#include "rtc.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void) {
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_read(rtc_time_t *t) {
    // Wait until RTC is not updating
    while (rtc_updating());

    uint8_t seconds = cmos_read(0x00);
    uint8_t minutes = cmos_read(0x02);
    uint8_t hours   = cmos_read(0x04);
    uint8_t day     = cmos_read(0x07);
    uint8_t month   = cmos_read(0x08);
    uint8_t year    = cmos_read(0x09);

    // Read again to confirm values (avoid RTC update glitch)
    uint8_t s2, m2, h2;
    do {
        s2 = seconds; m2 = minutes; h2 = hours;
        while (rtc_updating());
        seconds = cmos_read(0x00);
        minutes = cmos_read(0x02);
        hours   = cmos_read(0x04);
    } while (s2 != seconds || m2 != minutes || h2 != hours);

    // Check if BCD or binary mode (status register B, bit 2)
    uint8_t regB = cmos_read(0x0B);
    if (!(regB & 0x04)) {
        seconds = bcd_to_bin(seconds);
        minutes = bcd_to_bin(minutes);
        hours   = bcd_to_bin(hours);
        day     = bcd_to_bin(day);
        month   = bcd_to_bin(month);
        year    = bcd_to_bin(year);
    }

    // 12h → 24h conversion
    if (!(regB & 0x02) && (hours & 0x80)) {
        hours = ((hours & 0x7F) + 12) % 24;
    }

    t->seconds = seconds;
    t->minutes = minutes;
    t->hours   = hours;
    t->day     = day;
    t->month   = month;
    t->year    = year;
}

static void u8_to_2digits(uint8_t v, char *out) {
    out[0] = '0' + (v / 10);
    out[1] = '0' + (v % 10);
}

void rtc_format_time(const rtc_time_t *t, char *buf) {
    u8_to_2digits(t->hours,   buf + 0);
    buf[2] = ':';
    u8_to_2digits(t->minutes, buf + 3);
    buf[5] = ':';
    u8_to_2digits(t->seconds, buf + 6);
    buf[8] = '\0';
}
