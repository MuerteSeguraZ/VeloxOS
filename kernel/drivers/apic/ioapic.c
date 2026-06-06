#include "ioapic.h"

#define IOREGSEL  0x00
#define IOWIN     0x10
#define IOAPIC_VER   0x01
#define IOAPIC_RED   0x10

#define RTE_MASKED     (1u << 16)
#define RTE_LEVEL      (1u << 15)
#define RTE_ACTIVE_LOW (1u << 13)

static volatile uint32_t *g_base     = 0;
static uint32_t           g_gsi_base = 0;
static uint32_t           g_max_rte  = 0;

static ioapic_override_t g_ovr[24];
static int               g_ovr_count = 0;

static uint32_t io_read(uint8_t reg) {
    *(volatile uint32_t*)((uint8_t*)g_base + IOREGSEL) = reg;
    return *(volatile uint32_t*)((uint8_t*)g_base + IOWIN);
}
static void io_write(uint8_t reg, uint32_t val) {
    *(volatile uint32_t*)((uint8_t*)g_base + IOREGSEL) = reg;
    *(volatile uint32_t*)((uint8_t*)g_base + IOWIN)    = val;
}

static void rte_write(uint32_t gsi, uint64_t val) {
    uint8_t idx = (uint8_t)((gsi - g_gsi_base) * 2 + IOAPIC_RED);
    io_write(idx,     (uint32_t)(val & 0xFFFFFFFFu));
    io_write(idx + 1, (uint32_t)(val >> 32));
}
static uint64_t rte_read(uint32_t gsi) {
    uint8_t idx = (uint8_t)((gsi - g_gsi_base) * 2 + IOAPIC_RED);
    return (uint64_t)io_read(idx) | ((uint64_t)io_read(idx+1) << 32);
}

void ioapic_init(uint64_t phys_base, uint32_t gsi_base,
                 const ioapic_override_t *ovr, int count) {
    g_base     = (volatile uint32_t*)(uintptr_t)phys_base;
    g_gsi_base = gsi_base;
    g_max_rte  = ((io_read(IOAPIC_VER) >> 16) & 0xFF) + 1;

    g_ovr_count = count < 24 ? count : 24;
    for (int i = 0; i < g_ovr_count; i++) g_ovr[i] = ovr[i];

    for (uint32_t i = 0; i < g_max_rte; i++)
        rte_write(g_gsi_base + i, RTE_MASKED | 0xFFu);

    DPRINT("[IOAPIC] Init OK, entries="); DPRINT_HEX(g_max_rte); DPRINT("\n");
}

void ioapic_map_irq(uint8_t irq, uint8_t vector, uint8_t dest) {
    uint32_t gsi        = irq;
    int      active_low = 0;
    int      level      = 0;

    for (int i = 0; i < g_ovr_count; i++) {
        if (g_ovr[i].source_irq == irq) {
            gsi        = g_ovr[i].gsi;
            active_low = (g_ovr[i].flags & 0x3) == 0x3;
            level      = ((g_ovr[i].flags >> 2) & 0x3) == 0x3;
            break;
        }
    }

    uint64_t rte = (uint64_t)vector;
    if (active_low) rte |= RTE_ACTIVE_LOW;
    if (level)      rte |= RTE_LEVEL;
    rte |= ((uint64_t)dest << 56);
    rte_write(gsi, rte);
}

void ioapic_mask_irq(uint8_t irq) {
    uint32_t gsi = irq;
    for (int i=0;i<g_ovr_count;i++) if(g_ovr[i].source_irq==irq){gsi=g_ovr[i].gsi;break;}
    rte_write(gsi, rte_read(gsi) | RTE_MASKED);
}
void ioapic_unmask_irq(uint8_t irq) {
    uint32_t gsi = irq;
    for (int i=0;i<g_ovr_count;i++) if(g_ovr[i].source_irq==irq){gsi=g_ovr[i].gsi;break;}
    rte_write(gsi, rte_read(gsi) & ~(uint64_t)RTE_MASKED);
}