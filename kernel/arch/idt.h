#pragma once
#include "../stdint.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef void (*irq_handler_t)(void);

void idt_init(void);
void irq_register(int irq, irq_handler_t handler);
void irq_dispatch(int irq);
