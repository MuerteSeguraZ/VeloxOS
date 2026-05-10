#pragma once
#include "stdint.h"

void    lapic_init(uint64_t phys_base);
void    lapic_timer_init(uint32_t hz, uint8_t vector);
void    lapic_eoi(void);
uint8_t lapic_id(void);
void    lapic_send_ipi(uint8_t dest_apic_id, uint8_t vector);

/* Masks all 8259 IRQs and masks LINT0.
 * Call only after IOAPIC is routing all IRQs you care about. */
void    pic_disable(void);