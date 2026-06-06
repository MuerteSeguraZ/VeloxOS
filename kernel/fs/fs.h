#pragma once
#include "../stdint.h"

#define VFS_MAGIC            0x564C5847
#define VFS_VERSION          2
#define VFS_MAX_FILES        64
#define VFS_NAME_MAX         48
#define VFS_MAX_FILE_SECTORS 64
#define VFS_ROOT_PARENT      -1

#define SB_SECTOR          0
#define TABLE_SECTOR       1
#define TABLE_SECTORS      8
#define DATA_START         (TABLE_SECTOR + TABLE_SECTORS)
#define ENTRIES_PER_SECTOR (512 / sizeof(vfs_entry_t))

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t data_start_sector;
    uint8_t  _pad[512 - 16];
} __attribute__((packed)) vfs_superblock_t;

typedef struct {
    uint8_t  used;
    uint8_t  is_dir;
    int16_t  parent_idx;
    uint32_t start_sector;
    uint32_t size_bytes;
    uint32_t sector_count;
    uint8_t  _pad1[2];
    char     name[VFS_NAME_MAX];
} __attribute__((packed)) vfs_entry_t;

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
int      vfs_create(const char *name, int is_dir);
int      vfs_create_in(const char *name, int is_dir, int parent_idx);
int      vfs_write(int idx, const void *data, uint32_t size);
uint32_t vfs_read(int idx, void *buf);
int      vfs_find(const char *name);
int      vfs_find_in(const char *name, int parent_idx);
int      vfs_delete(int idx);
typedef void (*vfs_list_cb)(int idx, const vfs_entry_t *entry);
void     vfs_list(vfs_list_cb cb);
void     vfs_list_in(int parent_idx, vfs_list_cb cb);
int      vfs_flush(void);