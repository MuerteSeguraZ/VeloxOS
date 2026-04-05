#pragma once
#include "../stdint.h"

void mouse_init(void);

// Call once per frame — returns accumulated delta since last call
// Returns 1 if there's new data, 0 if nothing changed
int mouse_get_delta(int *dx, int *dy, int *btn_left);