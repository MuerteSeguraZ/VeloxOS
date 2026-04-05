#include "alloc.h"

static uint64_t heap_start = 0;
static uint64_t heap_end   = 0;
static uint64_t heap_ptr   = 0;

void mm_init(uint64_t start, uint64_t size) {
    heap_start = start;
    heap_end   = start + size;
    heap_ptr   = start;
}

void *mm_alloc(size_t bytes) {
    // 16-byte align
    uint64_t addr = (heap_ptr + 15) & ~(uint64_t)15;
    if (addr + bytes > heap_end) return 0;   // out of memory
    heap_ptr = addr + bytes;
    return (void *)addr;
}

void mm_reset(void) {
    heap_ptr = heap_start;
}

uint64_t mm_used(void) {
    return heap_ptr - heap_start;
}
