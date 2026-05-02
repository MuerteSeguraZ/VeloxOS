#include "fs.h"
#include "../drivers/ata/ata.h"

vfs_t vfs;

static void mem_set(void *dst, uint8_t val, uint32_t n) {
    uint8_t *p=(uint8_t*)dst; while(n--)*p++=val;
}
static void mem_cpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d=(uint8_t*)dst; const uint8_t *s=(const uint8_t*)src;
    while(n--)*d++=*s++;
}
static int str_eq(const char *a, const char *b) {
    while(*a&&*b&&*a==*b){a++;b++;} return *a==*b;
}
static int str_len(const char *s) { int n=0; while(*s++)n++; return n; }

static int flush_superblock(void) {
    return ata_write(vfs.bus,vfs.drive,SB_SECTOR,1,&vfs.sb);
}

static int flush_table(void) {
    uint8_t buf[512];
    for(uint32_t s=0;s<TABLE_SECTORS;s++){
        mem_set(buf,0,512);
        for(uint32_t e=0;e<ENTRIES_PER_SECTOR;e++){
            uint32_t idx=s*ENTRIES_PER_SECTOR+e;
            if(idx>=VFS_MAX_FILES)break;
            mem_cpy(buf+e*sizeof(vfs_entry_t),&vfs.entries[idx],sizeof(vfs_entry_t));
        }
        if(!ata_write(vfs.bus,vfs.drive,TABLE_SECTOR+s,1,buf))return 0;
    }
    return 1;
}

static int load_table(void) {
    uint8_t buf[512];
    for(uint32_t s=0;s<TABLE_SECTORS;s++){
        if(!ata_read(vfs.bus,vfs.drive,TABLE_SECTOR+s,1,buf))return 0;
        for(uint32_t e=0;e<ENTRIES_PER_SECTOR;e++){
            uint32_t idx=s*ENTRIES_PER_SECTOR+e;
            if(idx>=VFS_MAX_FILES)break;
            mem_cpy(&vfs.entries[idx],buf+e*sizeof(vfs_entry_t),sizeof(vfs_entry_t));
        }
    }
    return 1;
}

static uint32_t alloc_sectors(uint32_t count) {
    (void)count;
    uint32_t next=DATA_START;
    for(int i=0;i<VFS_MAX_FILES;i++){
        if(!vfs.entries[i].used)continue;
        uint32_t end=vfs.entries[i].start_sector+vfs.entries[i].sector_count;
        if(end>next)next=end;
    }
    return next;
}

// ── Internal create ───────────────────────────────────────────────────────────
static int create_entry(const char *name, int is_dir, int parent_idx) {
    if(!vfs.mounted)return -1;
    if(str_len(name)>=VFS_NAME_MAX)return -1;

    // No duplicates within same parent
    if(vfs_find_in(name,parent_idx)>=0)return -1;

    int idx=-1;
    for(int i=0;i<VFS_MAX_FILES;i++){if(!vfs.entries[i].used){idx=i;break;}}
    if(idx<0)return -1;

    vfs_entry_t *e=&vfs.entries[idx];
    mem_set(e,0,sizeof(*e));
    e->used         = 1;
    e->is_dir       = is_dir?1:0;
    e->parent_idx   = (int16_t)parent_idx;
    e->start_sector = alloc_sectors(is_dir?0:VFS_MAX_FILE_SECTORS);
    e->sector_count = is_dir?0:VFS_MAX_FILE_SECTORS;
    e->size_bytes   = 0;

    int i=0;
    while(name[i]&&i<VFS_NAME_MAX-1){e->name[i]=name[i];i++;}
    e->name[i]=0;

    vfs.sb.file_count++;
    if(!flush_table())      return -1;
    if(!flush_superblock()) return -1;
    return idx;
}

// ── Public API ────────────────────────────────────────────────────────────────

int vfs_format(int bus, int drive) {
    vfs.bus=bus; vfs.drive=drive; vfs.mounted=0;
    mem_set(&vfs.sb,0,sizeof(vfs.sb));
    vfs.sb.magic            =VFS_MAGIC;
    vfs.sb.version          =VFS_VERSION;
    vfs.sb.file_count       =0;
    vfs.sb.data_start_sector=DATA_START;
    mem_set(vfs.entries,0,sizeof(vfs.entries));
    if(!flush_superblock())return 0;
    if(!flush_table())     return 0;
    vfs.mounted=1;
    return 1;
}

int vfs_mount(int bus, int drive) {
    vfs.bus=bus; vfs.drive=drive; vfs.mounted=0;
    if(!ata_read(bus,drive,SB_SECTOR,1,&vfs.sb))return 0;
    if(vfs.sb.magic!=VFS_MAGIC)                  return 0;
    if(!load_table())                             return 0;
    vfs.mounted=1;
    return 1;
}

int vfs_create(const char *name, int is_dir) {
    return create_entry(name,is_dir,VFS_ROOT_PARENT);
}

int vfs_create_in(const char *name, int is_dir, int parent_idx) {
    // Validate parent is actually a directory
    if(parent_idx!=VFS_ROOT_PARENT){
        if(parent_idx<0||parent_idx>=VFS_MAX_FILES)return -1;
        if(!vfs.entries[parent_idx].used||!vfs.entries[parent_idx].is_dir)return -1;
    }
    return create_entry(name,is_dir,parent_idx);
}

int vfs_write(int idx, const void *data, uint32_t size) {
    if(!vfs.mounted||idx<0||idx>=VFS_MAX_FILES)return 0;
    vfs_entry_t *e=&vfs.entries[idx];
    if(!e->used||e->is_dir)return 0;
    uint32_t needed=(size+ATA_SECTOR_SIZE-1)/ATA_SECTOR_SIZE;
    if(needed>VFS_MAX_FILE_SECTORS)return 0;
    uint8_t buf[ATA_SECTOR_SIZE];
    const uint8_t *src=(const uint8_t*)data;
    uint32_t remaining=size;
    for(uint32_t s=0;s<needed;s++){
        uint32_t chunk=remaining>ATA_SECTOR_SIZE?ATA_SECTOR_SIZE:remaining;
        mem_cpy(buf,src,chunk);
        if(chunk<ATA_SECTOR_SIZE)mem_set(buf+chunk,0,ATA_SECTOR_SIZE-chunk);
        if(!ata_write(vfs.bus,vfs.drive,e->start_sector+s,1,buf))return 0;
        src+=chunk; remaining-=chunk;
    }
    e->size_bytes=size; e->sector_count=needed;
    if(!flush_table())return 0;
    return 1;
}

uint32_t vfs_read(int idx, void *buf) {
    if(!vfs.mounted||idx<0||idx>=VFS_MAX_FILES)return 0;
    vfs_entry_t *e=&vfs.entries[idx];
    if(!e->used||e->size_bytes==0)return 0;
    uint32_t sectors=(e->size_bytes+ATA_SECTOR_SIZE-1)/ATA_SECTOR_SIZE;
    if(!ata_read(vfs.bus,vfs.drive,e->start_sector,sectors,buf))return 0;
    return e->size_bytes;
}

int vfs_find(const char *name) {
    return vfs_find_in(name,VFS_ROOT_PARENT);
}

int vfs_find_in(const char *name, int parent_idx) {
    for(int i=0;i<VFS_MAX_FILES;i++){
        if(!vfs.entries[i].used)continue;
        if((int)vfs.entries[i].parent_idx!=parent_idx)continue;
        if(str_eq(vfs.entries[i].name,name))return i;
    }
    return -1;
}

int vfs_delete(int idx) {
    if(!vfs.mounted||idx<0||idx>=VFS_MAX_FILES)return 0;
    if(!vfs.entries[idx].used)return 0;

    // If it's a directory, also delete its children recursively
    if(vfs.entries[idx].is_dir){
        for(int i=0;i<VFS_MAX_FILES;i++){
            if(vfs.entries[i].used&&(int)vfs.entries[i].parent_idx==idx)
                vfs_delete(i);
        }
    }

    mem_set(&vfs.entries[idx],0,sizeof(vfs_entry_t));
    // parent_idx zero after memset — set to root explicitly
    vfs.entries[idx].parent_idx=(int16_t)VFS_ROOT_PARENT;
    vfs.sb.file_count--;
    if(!flush_table())      return 0;
    if(!flush_superblock()) return 0;
    return 1;
}

void vfs_list(vfs_list_cb cb) {
    vfs_list_in(VFS_ROOT_PARENT,cb);
}

void vfs_list_in(int parent_idx, vfs_list_cb cb) {
    for(int i=0;i<VFS_MAX_FILES;i++){
        if(!vfs.entries[i].used)continue;
        if((int)vfs.entries[i].parent_idx!=parent_idx)continue;
        cb(i,&vfs.entries[i]);
    }
}

int vfs_flush(void) {
    if(!vfs.mounted)return 0;
    if(!flush_table())      return 0;
    if(!flush_superblock()) return 0;
    return 1;
}