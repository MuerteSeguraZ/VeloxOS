#include "bus.h"
#include "../../graphics/text.h"

static bus_device_t devices[MAX_DEVICES];
static int device_count = 0;

static void str_cpy(char *d, const char *s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

void bus_register(const char *name, const char *type,
                  bus_probe_fn probe, bus_init_fn init) {
    if (device_count >= MAX_DEVICES) return;

    bus_device_t *d = &devices[device_count++];
    str_cpy((char *)d->name, name, 32);
    str_cpy((char *)d->type, type, 32);
    d->probe = probe;
    d->init = init;
    d->enabled = 1;
    d->probed = 0;
    d->probe_result = 0;
    d->initialized = 0;
}

int bus_enumerate(void) {
    int initialized = 0;

    for (int i = 0; i < device_count; i++) {
        bus_device_t *d = &devices[i];

        if (!d->enabled) continue;

        if (d->probe) {
            d->probe_result = d->probe();
            d->probed = 1;
            if (!d->probe_result) continue;
        } else {
            d->probe_result = 1;
            d->probed = 1;
        }

        if (d->init) {
            if (d->init()) {
                d->initialized = 1;
                initialized++;
            }
        }
    }

    return initialized;
}

bus_device_t *bus_find(const char *name) {
    for (int i = 0; i < device_count; i++) {
        if (str_eq(devices[i].name, name))
            return &devices[i];
    }
    return NULL;
}

bus_device_t *bus_find_by_type(const char *type, int index) {
    int count = 0;
    for (int i = 0; i < device_count; i++) {
        if (str_eq(devices[i].type, type)) {
            if (count == index)
                return &devices[i];
            count++;
        }
    }
    return NULL;
}

void bus_print_devices(void) {
    text_puts(0, 0, "=== Device Bus ===", 0xffffff, 0, 1);
    int y = 16;
    for (int i = 0; i < device_count; i++) {
        bus_device_t *d = &devices[i];
        char status[64];
        int si = 0;
        const char *p = d->name;
        while (*p && si < 32) { status[si++] = *p++; }
        status[si++] = ' ';
        status[si++] = '[';
        if (d->probed) {
            p = d->probe_result ? "FOUND" : "MISSING";
            while (*p && si < 62) { status[si++] = *p++; }
        } else {
            p = "ASSUMED";
            while (*p && si < 62) { status[si++] = *p++; }
        }
        status[si++] = ']';
        status[si++] = ' ';
        if (d->initialized) {
            p = "OK";
        } else if (d->enabled) {
            p = "FAIL";
        } else {
            p = "DISABLED";
        }
        while (*p && si < 62) { status[si++] = *p++; }
        status[si] = 0;

        uint32_t col = d->initialized ? 0x40ff40 : (d->enabled ? 0xff4040 : 0x888888);
        text_puts(0, y, status, col, 0, 1);
        y += 16;
    }
}

int bus_get_device_count(void) {
    return device_count;
}

bus_device_t *bus_get_device(int idx) {
    if (idx < 0 || idx >= device_count) return NULL;
    return &devices[idx];
}