#include "ata.h"

static ata_drive_t drives[2][2];  // [bus][drive]

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t r; __asm__ volatile ("inw %1, %0" : "=a"(r) : "Nd"(port)); return r;
}

static uint16_t bus_base(int bus) {
    return bus == ATA_PRIMARY ? ATA_PRIMARY_BASE : ATA_SECONDARY_BASE;
}
static uint16_t bus_ctrl(int bus) {
    return bus == ATA_PRIMARY ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
}

// 400ns delay — read alt status 4 times
static void ata_delay(int bus) {
    uint16_t ctrl = bus_ctrl(bus);
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);
}

// Wait until BSY clears, return final status
static uint8_t ata_wait_bsy(int bus) {
    uint16_t base = bus_base(bus);
    uint8_t  status;
    int timeout = 100000;
    while (timeout--) {
        status = inb(base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) break;
    }
    return status;
}

// Wait until DRQ or ERR
static uint8_t ata_wait_drq(int bus) {
    uint16_t base = bus_base(bus);
    uint8_t  status;
    int timeout = 100000;
    while (timeout--) {
        status = inb(base + ATA_REG_STATUS);
        if (status & (ATA_SR_DRQ | ATA_SR_ERR)) break;
    }
    return status;
}

// Select drive (master=0, slave=1), LBA mode
static void ata_select(int bus, int drive, uint32_t lba_high4) {
    uint16_t base = bus_base(bus);
    // 0xE0 = LBA mode | always-1 bits; drive bit = bit 4
    outb(base + ATA_REG_HDDEVSEL,
         0xE0 | ((drive & 1) << 4) | (lba_high4 & 0x0F));
    ata_delay(bus);
}

static void ata_identify_drive(int bus, int drive) {
    ata_drive_t *d = &drives[bus][drive];
    d->present = 0;

    uint16_t base = bus_base(bus);

    // Select drive
    ata_select(bus, drive, 0);

    // Send IDENTIFY
    outb(base + ATA_REG_SECCOUNT, 0);
    outb(base + ATA_REG_LBA0,     0);
    outb(base + ATA_REG_LBA1,     0);
    outb(base + ATA_REG_LBA2,     0);
    outb(base + ATA_REG_COMMAND,  ATA_CMD_IDENTIFY);

    // Read status — if 0 no drive
    uint8_t status = inb(base + ATA_REG_STATUS);
    if (status == 0) return;

    // Wait for BSY to clear
    ata_wait_bsy(bus);

    // Check LBA1/LBA2 — if non-zero it's not ATA (e.g. ATAPI)
    if (inb(base + ATA_REG_LBA1) || inb(base + ATA_REG_LBA2)) return;

    // Wait for DRQ
    status = ata_wait_drq(bus);
    if (status & ATA_SR_ERR) return;

    // Read 256 words of IDENTIFY data
    uint16_t identify[256];
    for (int i = 0; i < 256; i++)
        identify[i] = inw(base + ATA_REG_DATA);

    // Sectors (28-bit LBA) at words 60-61
    d->sectors = ((uint32_t)identify[61] << 16) | identify[60];

    // Model string at words 27-46 (each word is 2 chars, big-endian)
    int mi = 0;
    for (int w = 27; w <= 46; w++) {
        d->model[mi++] = (identify[w] >> 8) & 0xFF;
        d->model[mi++] = (identify[w]     ) & 0xFF;
    }
    // Trim trailing spaces
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--)
        d->model[i] = 0;

    d->present = 1;
}

void ata_init(void) {
    ata_identify_drive(ATA_PRIMARY,   ATA_MASTER);
    ata_identify_drive(ATA_PRIMARY,   ATA_SLAVE);
    ata_identify_drive(ATA_SECONDARY, ATA_MASTER);
    ata_identify_drive(ATA_SECONDARY, ATA_SLAVE);
}

ata_drive_t *ata_get_drive(int bus, int drive) {
    return &drives[bus][drive];
}

int ata_read(int bus, int drive, uint32_t lba, uint32_t count, void *buf) {
    uint16_t base = bus_base(bus);
    uint16_t *ptr = (uint16_t *)buf;

    while (count > 0) {
        uint32_t sectors = (count > 255) ? 255 : count;

        ata_wait_bsy(bus);
        ata_select(bus, drive, (lba >> 24) & 0x0F);

        outb(base + ATA_REG_SECCOUNT, (uint8_t)sectors);
        outb(base + ATA_REG_LBA0,     (uint8_t)(lba      ));
        outb(base + ATA_REG_LBA1,     (uint8_t)(lba >>  8));
        outb(base + ATA_REG_LBA2,     (uint8_t)(lba >> 16));
        outb(base + ATA_REG_COMMAND,  ATA_CMD_READ_PIO);

        for (uint32_t s = 0; s < sectors; s++) {
            uint8_t status = ata_wait_drq(bus);
            if (status & ATA_SR_ERR) return 0;

            for (int w = 0; w < 256; w++)
                *ptr++ = inw(base + ATA_REG_DATA);

            ata_delay(bus);
        }

        lba   += sectors;
        count -= sectors;
    }
    return 1;
}

int ata_write(int bus, int drive, uint32_t lba, uint32_t count, const void *buf) {
    uint16_t        base = bus_base(bus);
    const uint16_t *ptr  = (const uint16_t *)buf;

    while (count > 0) {
        uint32_t sectors = (count > 255) ? 255 : count;

        ata_wait_bsy(bus);
        ata_select(bus, drive, (lba >> 24) & 0x0F);

        outb(base + ATA_REG_SECCOUNT, (uint8_t)sectors);
        outb(base + ATA_REG_LBA0,     (uint8_t)(lba      ));
        outb(base + ATA_REG_LBA1,     (uint8_t)(lba >>  8));
        outb(base + ATA_REG_LBA2,     (uint8_t)(lba >> 16));
        outb(base + ATA_REG_COMMAND,  ATA_CMD_WRITE_PIO);

        for (uint32_t s = 0; s < sectors; s++) {
            uint8_t status = ata_wait_drq(bus);
            if (status & ATA_SR_ERR) return 0;

            for (int w = 0; w < 256; w++)
                outw(base + ATA_REG_DATA, *ptr++);

            ata_delay(bus);
        }

        // Flush write cache
        outb(base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
        ata_wait_bsy(bus);

        lba   += sectors;
        count -= sectors;
    }
    return 1;
}