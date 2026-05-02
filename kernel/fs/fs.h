#pragma once
#include "../stdint.h"

#define VFS_MAGIC            0x564C5847
#define VFS_VERSION          2
#define VFS_MAX_FILES        64
#define VFS_NAME_MAX         48
#define VFS_MAX_FILE_SECTORS 64
#define VFS_ROOT_PARENT      -1           // parent_idx value for root-level entries

#define SB_SECTOR          0
#define TABLE_SECTOR       1
#define TABLE_SECTORS      8
#define DATA_START         (TABLE_SECTOR + TABLE_SECTORS)
#define ENTRIES_PER_SECTOR (512 / sizeof(vfs_entry_t))

// Superblock — sector 0
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t data_start_sector;
    uint8_t  _pad[512 - 16];
} __attribute__((packed)) vfs_superblock_t;

// File entry — exactly 64 bytes
typedef struct {
    uint8_t  used;
    uint8_t  is_dir;
    int16_t  parent_idx;      // index of parent dir entry, or VFS_ROOT_PARENT (-1)
    uint32_t start_sector;
    uint32_t size_bytes;
    uint32_t sector_count;
    uint8_t  _pad1[2];        // was 4, now 2 (gave 2 bytes to parent_idx)
    char     name[VFS_NAME_MAX];
} __attribute__((packed)) vfs_entry_t;   // still 64 bytes

typedef struct {
    int              mounted;
    int              bus;
    int              drive;
    vfs_superblock_t sb;
    vfs_entry_t      entries[VFS_MAX_FILES];
} vfs_t;

extern vfs_t vfs;

int      vfs_format(int bus, int drive);
int      vfs_mount(int bus, int drive);

// Create at root level
int      vfs_create(const char *name, int is_dir);

// Create inside a directory (parent_idx = index of parent dir entry)
int      vfs_create_in(const char *name, int is_dir, int parent_idx);

int      vfs_write(int idx, const void *data, uint32_t size);
uint32_t vfs_read(int idx, void *buf);
int      vfs_find(const char *name);

// Find by name within a specific parent (-1 = root)
int      vfs_find_in(const char *name, int parent_idx);

int      vfs_delete(int idx);

typedef void (*vfs_list_cb)(int idx, const vfs_entry_t *entry);
void     vfs_list(vfs_list_cb cb);

// List only children of a given parent_idx
void     vfs_list_in(int parent_idx, vfs_list_cb cb);

int      vfs_flush(void);