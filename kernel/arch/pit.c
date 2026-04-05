#include "pit.h"
#include "idt.h"

#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43
#define PIT_BASE_HZ  1193182

static volatile uint64_t tick_count = 0;
static volatile int      tick_fired = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pit_irq_handler(void) {
    tick_count++;
    tick_fired = 1;
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;
    outb(PIT_CMD,      0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_register(0, pit_irq_handler);
}

void pit_wait_tick(void) {
    tick_fired = 0;
    while (!tick_fired)
        __asm__ volatile ("hlt");
}

uint64_t pit_ticks(void) {
    return tick_count;
}
