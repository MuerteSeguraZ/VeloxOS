#pragma once
#include "../../stdint.h"

typedef int (*bus_probe_fn)(void);

typedef int (*bus_init_fn)(void);

typedef struct {
    const char *name;
    const char *type;
    bus_probe_fn probe;
    bus_init_fn init;
    int enabled;
    int probed;
    int probe_result;
    int initialized;
} bus_device_t;

void bus_register(const char *name, const char *type,
                  bus_probe_fn probe, bus_init_fn init);

int bus_enumerate(void);
bus_device_t *bus_find(const char *name);
bus_device_t *bus_find_by_type(const char *type, int index);
void bus_print_devices(void);
int bus_get_device_count(void);
bus_device_t *bus_get_device(int idx);