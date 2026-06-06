#pragma once
#include "../../stdint.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} rtc_time_t;

void rtc_read(rtc_time_t *t);

void rtc_format_time(const rtc_time_t *t, char *buf);
