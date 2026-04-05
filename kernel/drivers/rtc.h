#pragma once
#include "../stdint.h"

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;   // 2-digit year
} rtc_time_t;

void rtc_read(rtc_time_t *t);

// Format "HH:MM:SS" into buf (must be at least 9 bytes)
void rtc_format_time(const rtc_time_t *t, char *buf);
