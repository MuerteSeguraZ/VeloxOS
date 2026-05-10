#pragma once
#include "stdint.h"

#define ACPI_MAX_OVERRIDES 24

typedef struct {
    uint8_t  bus;
    uint8_t  source_irq;
    uint32_t gsi;
    uint16_t flags;
} acpi_override_t;

typedef struct {
    uint64_t lapic_phys;
    uint64_t ioapic_phys;
    uint32_t ioapic_gsi_base;
    acpi_override_t overrides[ACPI_MAX_OVERRIDES];
    int      override_count;
    int      cpu_count;
    uint8_t  lapic_ids[64];
} acpi_info_t;

int          acpi_init(uint64_t rsdp_hint);
acpi_info_t *acpi_get_info(void);