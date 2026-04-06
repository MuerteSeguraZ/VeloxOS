#pragma once

static inline int ata_bus_init(void) {
    ata_init();
    return 1;
}

#define ata_bus_probe NULL

static inline int mouse_bus_init(void) {
    mouse_init();
    return 1;
}

#define mouse_bus_probe NULL

static inline int rtc_bus_init(void) {
    return 1;
}

#define rtc_bus_probe NULL