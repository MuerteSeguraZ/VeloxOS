#pragma once
#include "../stdint.h"

typedef struct block {
    uint32_t       size;    // size of user data (not including header)
    uint32_t       free;    // 1 = free, 0 = used
    struct block  *next;    // next block in free list (only valid when free=1)
    uint32_t       magic;   // 0xVLXA = valid, detect corruption
} block_t;

#define BLOCK_MAGIC  0x564C5841   // "VLXA"
#define HEADER_SIZE  sizeof(block_t)
#define MIN_SPLIT    (HEADER_SIZE + 16)

void     mm_init(uint64_t start, uint64_t size);
void    *mm_alloc(size_t bytes);
void     mm_free(void *ptr);
uint64_t mm_used(void);
uint64_t mm_total(void);