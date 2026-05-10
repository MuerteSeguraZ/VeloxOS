#pragma once
#include "stdint.h"

typedef struct {
    uint8_t  source_irq;
    uint32_t gsi;
    uint16_t flags;
} ioapic_override_t;

/*
 * Initialise the I/O APIC and mask every entry.
 * Nothing is routed until you explicitly call ioapic_map_irq().
 * The legacy PIC is left completely alone.
 */
void ioapic_init(uint64_t phys_base, uint32_t gsi_base,
                 const ioapic_override_t *overrides, int count);

/*
 * Route a legacy ISA IRQ to an IDT vector on the given CPU.
 * Applies ACPI overrides automatically.
 * Only call this for IRQs you have a working IDT handler for.
 */
void ioapic_map_irq(uint8_t irq, uint8_t vector, uint8_t dest_lapic_id);

void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);