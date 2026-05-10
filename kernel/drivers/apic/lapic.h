#pragma once
#include "stdint.h"

/*
 * Initialise the Local APIC.
 * Does NOT touch the legacy 8259 PIC — keyboard and mouse keep working.
 */
void    lapic_init(uint64_t phys_base);

/*
 * Start the LAPIC periodic timer.
 * Use a vector well away from the PIC range (0x20-0x2F).
 * Recommended: 0x40.  Your IDT must have a handler registered for it.
 */
void    lapic_timer_init(uint32_t hz, uint8_t vector);

/* Call at the end of every LAPIC-delivered interrupt handler */
void    lapic_eoi(void);

uint8_t lapic_id(void);
void    lapic_send_ipi(uint8_t dest_apic_id, uint8_t vector);