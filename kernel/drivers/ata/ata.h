#pragma once
#include "../../stdint.h"

typedef struct {
    int      present;
    uint32_t sectors;
    char     model[41];
} ata_drive_t;

#define ATA_SECTOR_SIZE 512

#define ATA_PRIMARY   0
#define ATA_SECONDARY 1
#define ATA_MASTER    0
#define ATA_SLAVE     1

#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_FLUSH       0xE7

void ata_init(void);
ata_drive_t *ata_get_drive(int bus, int drive);
int ata_read(int bus, int drive, uint32_t lba, uint32_t count, void *buf);
int ata_write(int bus, int drive, uint32_t lba, uint32_t count, const void *buf);
