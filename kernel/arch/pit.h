#pragma once
#include "../stdint.h"

#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43
#define PIT_BASE_HZ  1193182

void     pit_init(uint32_t hz);
void     pit_wait_tick(void);
uint64_t pit_ticks(void);
