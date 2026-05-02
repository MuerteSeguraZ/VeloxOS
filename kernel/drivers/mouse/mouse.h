#pragma once
#include "../../stdint.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

void mouse_init(void);

// Returns 1 if there's new data, 0 if nothing changed
// btn_left and btn_right are 1 while held
int mouse_get_delta(int *dx, int *dy, int *btn_left, int *btn_right);