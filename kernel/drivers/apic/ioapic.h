#pragma once
#include "stdint.h"

typedef struct {
    uint8_t  source_irq;
    uint32_t gsi;
    uint16_t flags;
} ioapic_override_t;

void ioapic_init(uint64_t phys_base, uint32_t gsi_base,
                 const ioapic_override_t *overrides, int count);
void ioapic_map_irq(uint8_t irq, uint8_t vector, uint8_t dest_lapic_id);
void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);