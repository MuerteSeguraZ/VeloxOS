#include "lapic.h"

#define LAPIC_ID        0x020
#define LAPIC_VER       0x030
#define LAPIC_TPR       0x080
#define LAPIC_EOI_REG   0x0B0
#define LAPIC_SVR       0x0F0
#define LAPIC_ESR       0x280
#define LAPIC_ICR_LO    0x300
#define LAPIC_ICR_HI    0x310
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERR   0x370
#define LAPIC_TIMER_ICR 0x380
#define LAPIC_TIMER_CCR 0x390
#define LAPIC_TIMER_DIV 0x3E0

#define SVR_ENABLE      (1u << 8)
#define LVT_MASKED      (1u << 16)
#define LVT_PERIODIC    (1u << 17)
#define TIMER_DIV_16    0x3u

/* PIT channel 2 — used only during calibration, left alone afterward */
#define PIT_CH2   0x42
#define PIT_CMD   0x43
#define PIT_GATE  0x61

static volatile uint32_t *g_lapic = 0;

static uint32_t reg_read(uint32_t off)          { return g_lapic[off/4]; }
static void     reg_write(uint32_t off, uint32_t v) {
    g_lapic[off/4] = v;
    (void)g_lapic[LAPIC_ID/4];   /* serialise */
}

/* Measure LAPIC bus frequency using PIT channel 2 (~10 ms gate) */
static uint32_t calibrate_ticks_per_ms(void) {
    /* Program PIT ch2: mode 0, ~10 ms */
    outb(PIT_CMD,  0xB0);
    outb(PIT_CH2,  (uint8_t)(11932 & 0xFF));
    outb(PIT_CH2,  (uint8_t)(11932 >> 8));

    /* Arm LAPIC counter to max, divisor 16 */
    reg_write(LAPIC_TIMER_DIV, TIMER_DIV_16);
    reg_write(LAPIC_TIMER_ICR, 0xFFFFFFFFu);

    /* Start PIT ch2 gate */
    uint8_t gate = inb(PIT_GATE) & ~0x02u;
    outb(PIT_GATE, gate & ~0x01u);
    outb(PIT_GATE, gate |  0x01u);

    /* Wait for OUT pin to go high (~10 ms) */
    while (!(inb(PIT_GATE) & 0x20));

    uint32_t elapsed = 0xFFFFFFFFu - reg_read(LAPIC_TIMER_CCR);
    return elapsed / 10;   /* ticks per ms */
}

void lapic_init(uint64_t phys_base) {
    g_lapic = (volatile uint32_t *)(uintptr_t)phys_base;

    reg_write(LAPIC_SVR, 0xFFu | SVR_ENABLE);
    reg_write(LAPIC_TPR, 0);

    /* LINT0: ExtINT — PIC interrupts flow through here to the CPU */
    reg_write(LAPIC_LVT_LINT0, 0x700);
    /* LINT1: NMI */
    reg_write(LAPIC_LVT_LINT1, 0x400);
    /* Mask error vector */
    reg_write(LAPIC_LVT_ERR, LVT_MASKED);

    reg_write(LAPIC_ESR,     0);
    reg_write(LAPIC_ESR,     0);
    reg_write(LAPIC_EOI_REG, 0);

    DPRINT("[LAPIC] Init OK, ID="); DPRINT_HEX(lapic_id()); DPRINT("\n");
}

void lapic_timer_init(uint32_t hz, uint8_t vector) {
    /* vector must be >= 0x30 and must NOT overlap 0x20-0x2F (PIC range) */
    uint32_t tpm   = calibrate_ticks_per_ms();
    uint32_t count = (tpm * 1000u) / hz;

    DPRINT("[LAPIC] Timer ticks/ms="); DPRINT_HEX(tpm);
    DPRINT(" vector="); DPRINT_HEX(vector); DPRINT("\n");

    reg_write(LAPIC_TIMER_DIV, TIMER_DIV_16);
    reg_write(LAPIC_LVT_TIMER, (uint32_t)vector | LVT_PERIODIC);
    reg_write(LAPIC_TIMER_ICR, count);
}

void lapic_eoi(void)        { reg_write(LAPIC_EOI_REG, 0); }
uint8_t lapic_id(void)      { return (uint8_t)(reg_read(LAPIC_ID) >> 24); }

void lapic_send_ipi(uint8_t dest, uint8_t vec) {
    reg_write(LAPIC_ICR_HI, (uint32_t)dest << 24);
    reg_write(LAPIC_ICR_LO, (uint32_t)vec | (1u << 14));
    while (reg_read(LAPIC_ICR_LO) & (1u << 12));
}