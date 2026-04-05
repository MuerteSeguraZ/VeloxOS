#pragma once
#include "../stdint.h"

// ── VeloxFS layout on disk ────────────────────────────────────────────────────
//
//  Sector 0:        Superblock (magic, file count, version)
//  Sectors 1-8:     File table (up to 64 file entries, 64 bytes each)
//  Sectors 9+:      File data (each file gets a contiguous run of sectors)
//
// Max files:        64
// Max file size:    ~4MB (configurable via FS_MAX_FILE_SECTORS)
// Max total disk:   anything the ATA driver can handle

#define VFS_MAGIC           0x564C5846   // "VLXF"
#define VFS_VERSION         1
#define VFS_MAX_FILES       64
#define VFS_NAME_MAX        48
#define VFS_MAX_FILE_SECTORS 64          // 64 * 512 = 32KB per file max

// ── On-disk structures (all little-endian, 512-byte aligned) ─────────────────

// Superblock — sector 0
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t data_start_sector;   // first sector available for file data
    uint8_t  _pad[512 - 16];
} __attribute__((packed)) vfs_superblock_t;

// File entry — 64 bytes, up to 64 entries in sectors 1-8
typedef struct {
    uint8_t  used;                        // 1 if slot occupied
    uint8_t  is_dir;                      // 1 if directory
    uint8_t  _pad0[2];
    uint32_t start_sector;                // LBA of first data sector
    uint32_t size_bytes;                  // file size in bytes
    uint32_t sector_count;                // sectors allocated
    uint8_t  _pad1[4];
    char     name[VFS_NAME_MAX];          // null-terminated filename
} __attribute__((packed)) vfs_entry_t;   // 64 bytes

// ── In-memory FS state ────────────────────────────────────────────────────────
typedef struct {
    int             mounted;
    int             bus;
    int             drive;
    vfs_superblock_t sb;
    vfs_entry_t     entries[VFS_MAX_FILES];
} vfs_t;

extern vfs_t vfs;

// ── API ───────────────────────────────────────────────────────────────────────

// Format disk with a fresh VeloxFS (wipes everything)
int  vfs_format(int bus, int drive);

// Mount existing VeloxFS from disk
// Returns 1 on success, 0 if no valid FS found
int  vfs_mount(int bus, int drive);

// Create a file (or directory if is_dir=1)
// Returns file index on success, -1 on error
int  vfs_create(const char *name, int is_dir);

// Write data to a file (overwrites)
// Returns 1 on success, 0 on error
int  vfs_write(int idx, const void *data, uint32_t size);

// Read file data into buf (buf must be at least entry->size_bytes)
// Returns bytes read, 0 on error
uint32_t vfs_read(int idx, void *buf);

// Find file by name, returns index or -1
int  vfs_find(const char *name);

// Delete file by index
int  vfs_delete(int idx);

// List all files (calls cb for each used entry)
typedef void (*vfs_list_cb)(int idx, const vfs_entry_t *entry);
void vfs_list(vfs_list_cb cb);

// Flush entry table + superblock to disk
int  vfs_flush(void);