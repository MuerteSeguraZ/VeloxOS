#include "stdint.h"
#include "graphics/framebuffer.h"
#include "ui/desktop.h"
#include "drivers/mouse.h"
#include "drivers/ata.h"
#include "arch/idt.h"
#include "arch/pit.h"
#include "mm/alloc.h"
#include "fs/fs.h"

#define MB2_TAG_END         0
#define MB2_TAG_FRAMEBUFFER 8

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) mb2_tag_framebuffer_t;

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

#define HEAP_START 0x1000000ULL
#define HEAP_SIZE  0x2000000ULL

void kernel_main(uint32_t mb2_info_phys) {
    mb2_info_t *info = (mb2_info_t*)(uint64_t)mb2_info_phys;
    mb2_tag_t  *tag  = (mb2_tag_t*)((uint8_t*)info + 8);

    uint64_t fb_addr   = 0;
    uint32_t fb_pitch  = 0;
    uint32_t fb_width  = 0;
    uint32_t fb_height = 0;
    uint8_t  fb_bpp    = 32;

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_FRAMEBUFFER) {
            mb2_tag_framebuffer_t *fbt = (mb2_tag_framebuffer_t*)tag;
            fb_addr   = fbt->framebuffer_addr;
            fb_pitch  = fbt->framebuffer_pitch;
            fb_width  = fbt->framebuffer_width;
            fb_height = fbt->framebuffer_height;
            fb_bpp    = fbt->framebuffer_bpp;
        }
        tag = (mb2_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7u));
    }

    if (!fb_addr) {
        fb_addr   = 0xfd000000;
        fb_pitch  = 1024 * 4;
        fb_width  = 1024;
        fb_height = 768;
        fb_bpp    = 32;
    }

    mm_init(HEAP_START, HEAP_SIZE);
    void *backbuf = mm_alloc(fb_width * fb_height * 4);

    fb_init(fb_addr, fb_pitch, fb_width, fb_height, fb_bpp, backbuf);
    idt_init();
    pit_init(60);
    mouse_init();

    // ── ATA + Filesystem init ─────────────────────────────────────────────────
    ata_init();

    // Scan all 4 ATA positions — use first drive found
    ata_drive_t *disk = 0;
    int found_bus = -1, found_drv = -1;
    for (int b = 0; b <= 1 && !disk; b++) {
        for (int d = 0; d <= 1 && !disk; d++) {
            ata_drive_t *c = ata_get_drive(b, d);
            if (c->present) {
                disk      = c;
                found_bus = b;
                found_drv = d;
            }
        }
    }

    int fs_ok = 0;
    if (disk) {
        if (!vfs_mount(found_bus, found_drv))
            vfs_format(found_bus, found_drv);
        fs_ok = 1;

        // Write a hello file on first boot
        if (vfs_find("velox.txt") < 0) {
            int idx = vfs_create("velox.txt", 0);
            if (idx >= 0) {
                const char *msg =
                    "Welcome to Velox OS!\n"
                    "This file was created on first boot.\n";
                uint32_t len = 0;
                while (msg[len]) len++;
                vfs_write(idx, msg, len);
            }
        }
    }

    // ── Desktop ───────────────────────────────────────────────────────────────
    desktop_init();
    desktop_add_window(80,  60,  340, 220,
                       fs_ok ? "Welcome - Disk OK" : "Welcome - No Disk");
    desktop_add_window(450, 130, 300, 200, "About Velox");
    desktop_redraw();

    uint64_t last_clock_tick = 0;

    while (1) {
        pit_wait_tick();

        int dx, dy, btn;
        if (mouse_get_delta(&dx, &dy, &btn))
            desktop_mouse_move(dx, dy, btn);

        uint64_t now = pit_ticks();

        if (desktop.needs_full_redraw) {
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