#pragma once
#include "../../stdint.h"

// ATA drive info
typedef struct {
    int      present;       // 1 if drive was detected
    uint32_t sectors;       // total 28-bit LBA sectors
    char     model[41];     // model string from IDENTIFY
} ata_drive_t;

// Bus / drive selectors
#define ATA_PRIMARY   0
#define ATA_SECONDARY 1
#define ATA_MASTER    0
#define ATA_SLAVE     1

// Init — detects all drives on primary + secondary bus
void ata_init(void);

// Get drive info (bus = ATA_PRIMARY/SECONDARY, drive = ATA_MASTER/SLAVE)
ata_drive_t *ata_get_drive(int bus, int drive);

// Read `count` 512-byte sectors starting at `lba` into `buf`
// Returns 1 on success, 0 on error
int ata_read(int bus, int drive, uint32_t lba, uint32_t count, void *buf);

// Write `count` 512-byte sectors starting at `lba` from `buf`
// Returns 1 on success, 0 on error
int ata_write(int bus, int drive, uint32_t lba, uint32_t count, const void *buf);

#define ATA_SECTOR_SIZE 512