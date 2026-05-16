#include "velox.h"
#include "arch/acpi.h"
#include "drivers/apic/lapic.h"
#include "drivers/apic/ioapic.h"
#include "sched/scheduler.h"

#define MB2_TAG_END         0
#define MB2_TAG_FRAMEBUFFER 8
#define MB2_TAG_ACPI1       14
#define MB2_TAG_ACPI2       15

typedef struct { uint32_t type, size; }
    __attribute__((packed)) mb2_tag_t;
typedef struct {
    uint32_t type, size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t  framebuffer_bpp, framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) mb2_tag_framebuffer_t;
typedef struct {
    uint32_t type, size;
    uint8_t  rsdp[];
} __attribute__((packed)) mb2_tag_acpi_t;
typedef struct { uint32_t total_size, reserved; }
    __attribute__((packed)) mb2_info_t;

/* Declared in idt.c */
extern void lapic_vector_register(uint8_t vec, irq_handler_t handler);

#define HEAP_START 0x1000000ULL
#define HEAP_SIZE  0x2000000ULL

static void lapic_timer_handler(void) {
    sched_tick();
}

void kernel_main(uint32_t mb2_info_phys) {
    mb2_info_t *info = (mb2_info_t *)(uint64_t)mb2_info_phys;
    mb2_tag_t  *tag  = (mb2_tag_t *)((uint8_t *)info + 8);

    uint64_t fb_addr = 0;
    uint32_t fb_pitch = 0, fb_width = 0, fb_height = 0;
    uint8_t  fb_bpp = 32;
    uint64_t rsdp_addr = 0;

    while (tag->type != MB2_TAG_END) {
        switch (tag->type) {
        case MB2_TAG_FRAMEBUFFER: {
            mb2_tag_framebuffer_t *f = (mb2_tag_framebuffer_t *)tag;
            fb_addr   = f->framebuffer_addr;
            fb_pitch  = f->framebuffer_pitch;
            fb_width  = f->framebuffer_width;
            fb_height = f->framebuffer_height;
            fb_bpp    = f->framebuffer_bpp;
            break;
        }
        case MB2_TAG_ACPI1:
            if (!rsdp_addr)
                rsdp_addr = (uint64_t)(uintptr_t)((mb2_tag_acpi_t *)tag)->rsdp;
            break;
        case MB2_TAG_ACPI2:
            rsdp_addr = (uint64_t)(uintptr_t)((mb2_tag_acpi_t *)tag)->rsdp;
            break;
        }
        tag = (mb2_tag_t *)((uint8_t *)tag + ((tag->size + 7) & ~7u));
    }

    if (!fb_addr) {
        fb_addr = 0xfd000000; fb_pitch = 1024*4;
        fb_width = 1024; fb_height = 768; fb_bpp = 32;
    }

    mm_init(HEAP_START, HEAP_SIZE);
    void *backbuf = mm_alloc(fb_width * fb_height * 4);

    fb_init(fb_addr, fb_pitch, fb_width, fb_height, fb_bpp, backbuf);
    idt_init();
    sched_init();
    pit_init(60);

    DPRINT_INIT();
    DPRINT("\n=== Velox OS Kernel Boot ===\n\n");

    /* ── ACPI + APIC ─────────────────────────────────────────────────── */
    if (acpi_init(rsdp_addr) == 0) {
        acpi_info_t *acpi = acpi_get_info();

        lapic_init(acpi->lapic_phys);
        lapic_vector_register(0x40, lapic_timer_handler);
        lapic_timer_init(100, 0x40);

        if (acpi->ioapic_phys) {
            ioapic_override_t ov[ACPI_MAX_OVERRIDES];
            for (int i = 0; i < acpi->override_count; i++) {
                ov[i].source_irq = acpi->overrides[i].source_irq;
                ov[i].gsi        = acpi->overrides[i].gsi;
                ov[i].flags      = acpi->overrides[i].flags;
            }
            ioapic_init(acpi->ioapic_phys, acpi->ioapic_gsi_base,
                        ov, acpi->override_count);

            uint8_t bsp = lapic_id();

            ioapic_map_irq(0,  0x20, bsp);   /* PIT timer  */
            ioapic_map_irq(1,  0x21, bsp);   /* keyboard   */
            ioapic_map_irq(12, 0x2C, bsp);   /* PS/2 mouse */

            pic_disable();
            idt_enable_apic_eoi();

            DPRINT("IOAPIC routing active, 8259 PIC disabled\n\n");
        }
    } else {
        DPRINT("ACPI not found, using PIT/PIC only\n\n");
    }

    pit_init(60);

    /* ── PCI ─────────────────────────────────────────────────────────── */
    DPRINT("Initializing PCI bus...\n");
    pci_init();
    int pci_count = pci_enumerate();
    (void)pci_count;
    DPRINT("\n");

    /* ── Bus / device layer ──────────────────────────────────────────── */
    DPRINT("Registering system devices...\n");
    bus_register("ata-primary",   "storage", ata_bus_probe,   ata_bus_init);
    bus_register("ata-secondary", "storage", ata_bus_probe,   ata_bus_init);
    bus_register("mouse-ps2",     "input",   mouse_bus_probe, mouse_bus_init);
    bus_register("rtc-cmos",      "rtc",     rtc_bus_probe,   rtc_bus_init);

    DPRINT("Enumerating and initializing devices...\n");
    int devices_init = bus_enumerate();
    (void)devices_init;
    DPRINT("Device initialization complete\n\n");

    /* ── Filesystem ──────────────────────────────────────────────────── */
    DPRINT("Searching for storage devices...\n");
    ata_drive_t *disk = 0; int found_bus = -1, found_drv = -1;
    for (int b = 0; b <= 1 && !disk; b++)
        for (int d = 0; d <= 1 && !disk; d++) {
            ata_drive_t *c = ata_get_drive(b, d);
            if (c->present) { disk = c; found_bus = b; found_drv = d; }
        }
    int fs_ok = 0;
    if (disk) {
        DPRINT("Found disk at ATA ");
        DPRINT_HEX(found_bus); DPRINT(":"); DPRINT_HEX(found_drv);
        DPRINT(" - Mounting filesystem\n");
        if (!vfs_mount(found_bus, found_drv)) vfs_format(found_bus, found_drv);
        fs_ok = 1;
        if (vfs_find("velox.txt") < 0) {
            int idx = vfs_create("velox.txt", 0);
            if (idx >= 0) {
                const char *msg =
                    "Welcome to Velox OS!\n"
                    "This file was created on first boot.\n"
                    "You can edit and save this file.\n";
                uint32_t len = 0; while (msg[len]) len++;
                vfs_write(idx, msg, len);
                DPRINT("Created velox.txt on boot\n");
            }
        }
        DPRINT("Filesystem ready\n");
    } else {
        DPRINT("No storage device found\n");
    }
    DPRINT("\n");

    /* ── UI ──────────────────────────────────────────────────────────── */
    DPRINT("Initializing desktop...\n");
    desktop_init();
    desktop_add_window(80,  60,  340, 220, fs_ok ? "Welcome - Disk OK" : "Welcome - No Disk");
    desktop_add_window(450, 130, 300, 200, "About Velox");
    desktop_redraw();
    DPRINT("Desktop ready\n");
    DPRINT("\n=== Boot Complete ===\n\n");

    uint64_t last_clock_tick = 0;

    /* ── Main event loop ─────────────────────────────────────────────── */
    while (1) {
        pit_wait_tick();

        key_event_t evt;
        while (kbd_poll(&evt))
            desktop_handle_key(&evt);

        int dx, dy, bl, br;
        if (mouse_get_delta(&dx, &dy, &bl, &br))
            desktop_mouse_move(dx, dy, bl, br);

        uint64_t now = pit_ticks();

        if (desktop.needs_full_redraw) {
            desktop_redraw();
            last_clock_tick = now;
        } else if (input_box.dirty) {
            input_box.dirty = 0;
            desktop_redraw();
            last_clock_tick = now;
        } else if (desktop.dirty) {
            desktop_update_cursor();
            desktop.dirty = 0;
        }

        if (now - last_clock_tick >= 60) {
            desktop.needs_full_redraw = 1;
            last_clock_tick = now;
        }
    }
}