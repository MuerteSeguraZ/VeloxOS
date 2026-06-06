#pragma once
#include "../stdint.h"

typedef struct block {
    uint32_t       size;
    uint32_t       free;
    struct block  *next;
    uint32_t       magic;
} block_t;

#define BLOCK_MAGIC  0x564C5841
#define HEADER_SIZE  sizeof(block_t)
#define MIN_SPLIT    (HEADER_SIZE + 16)

void     mm_init(uint64_t start, uint64_t size);
void    *mm_alloc(size_t bytes);
void     mm_free(void *ptr);
uint64_t mm_used(void);
uint64_t mm_total(void);