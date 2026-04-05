#pragma once
#include "../stdint.h"

void mouse_init(void);

// Returns 1 if there's new data, 0 if nothing changed
// btn_left and btn_right are 1 while held
int mouse_get_delta(int *dx, int *dy, int *btn_left, int *btn_right);