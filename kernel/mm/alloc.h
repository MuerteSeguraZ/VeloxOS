#pragma once
#include "../stdint.h"

void  mm_init(uint64_t start, uint64_t size);
void *mm_alloc(size_t bytes);
void  mm_reset(void);          // free everything (used for init only)
uint64_t mm_used(void);
