#include "alloc.h"

static uint64_t heap_start = 0;
static uint64_t heap_size  = 0;
static block_t *free_list  = 0;   // head of free list

static void *ptr_add(void *p, uint64_t n) { return (uint8_t*)p + n; }

void mm_init(uint64_t start, uint64_t size) {
    heap_start = start;
    heap_size  = size;

    // Create one giant free block covering the whole heap
    block_t *b = (block_t *)start;
    b->size    = (uint32_t)(size - HEADER_SIZE);
    b->free    = 1;
    b->next    = 0;
    b->magic   = BLOCK_MAGIC;
    free_list  = b;
}

void *mm_alloc(size_t bytes) {
    if (!bytes) return 0;

    // Align to 16 bytes
    bytes = (bytes + 15) & ~(size_t)15;

    // First-fit search through free list
    block_t *prev = 0;
    block_t *cur  = free_list;

    while (cur) {
        if (cur->magic != BLOCK_MAGIC) return 0;  // heap corruption
        if (cur->size >= bytes) {
            // Found a fit — split if there's enough room left over
            if (cur->size >= bytes + MIN_SPLIT) {
                block_t *split = (block_t *)ptr_add(cur+1, bytes);
                split->size    = cur->size - bytes - HEADER_SIZE;
                split->free    = 1;
                split->magic   = BLOCK_MAGIC;
                split->next    = cur->next;
                cur->size      = (uint32_t)bytes;
                cur->next      = split;
                // Insert split into free list in place of cur
                if (prev) prev->next = split;
                else       free_list = split;
            } else {
                // Use whole block
                if (prev) prev->next = cur->next;
                else       free_list = cur->next;
            }
            cur->free = 0;
            cur->next = 0;
            return (void *)(cur + 1);
        }
        prev = cur;
        cur  = cur->next;
    }
    return 0;   // out of memory
}

void mm_free(void *ptr) {
    if (!ptr) return;

    block_t *b = (block_t *)ptr - 1;
    if (b->magic != BLOCK_MAGIC) return;  // invalid pointer
    if (b->free) return;                   // double free

    b->free = 1;

    // Insert back into free list in address order and coalesce
    block_t *prev = 0;
    block_t *cur  = free_list;

    while (cur && cur < b) {
        prev = cur;
        cur  = cur->next;
    }

    // Insert b between prev and cur
    b->next = cur;
    if (prev) prev->next = b;
    else       free_list = b;

    // Coalesce b with next if adjacent
    if (b->next) {
        block_t *next_blk = (block_t *)ptr_add(b+1, b->size);
        if (next_blk == b->next) {
            b->size += HEADER_SIZE + b->next->size;
            b->next  = b->next->next;
        }
    }

    // Coalesce prev with b if adjacent
    if (prev) {
        block_t *next_blk = (block_t *)ptr_add(prev+1, prev->size);
        if (next_blk == b) {
            prev->size += HEADER_SIZE + b->size;
            prev->next  = b->next;
        }
    }
}

uint64_t mm_used(void) {
    // Walk all blocks (free + used) and sum used sizes
    uint64_t used = 0;
    uint8_t *p = (uint8_t *)heap_start;
    uint8_t *end = p + heap_size;
    while (p < end) {
        block_t *b = (block_t *)p;
        if (b->magic != BLOCK_MAGIC) break;
        if (!b->free) used += HEADER_SIZE + b->size;
        p += HEADER_SIZE + b->size;
    }
    return used;
}

uint64_t mm_total(void) { return heap_size; }