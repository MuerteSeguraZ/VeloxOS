#include "acpi.h"

static acpi_info_t g_acpi;

typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32_t length;
    uint8_t  revision, checksum;
    char     oem_id[6], oem_table_id[8];
    uint32_t oem_revision, creator_id, creator_revision;
} sdt_header_t;

typedef struct __attribute__((packed)) {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} rsdp_t;

typedef struct __attribute__((packed)) {
    sdt_header_t header;
    uint32_t     lapic_address;
    uint32_t     flags;
} madt_t;

typedef struct __attribute__((packed)) { uint8_t type, length; } madt_hdr_t;
typedef struct __attribute__((packed)) { madt_hdr_t h; uint8_t proc_id, apic_id; uint32_t flags; }                    madt_lapic_t;
typedef struct __attribute__((packed)) { madt_hdr_t h; uint8_t id, res; uint32_t address, gsi_base; }                 madt_ioapic_t;
typedef struct __attribute__((packed)) { madt_hdr_t h; uint8_t bus, source; uint32_t gsi; uint16_t flags; }           madt_override_t;

static int checksum_ok(const void *p, uint32_t len) {
    const uint8_t *b = (const uint8_t *)p;
    uint8_t s = 0; for (uint32_t i = 0; i < len; i++) s += b[i];
    return s == 0;
}
static int sig4eq(const char *a, const char *b) {
    return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];
}

static rsdp_t *scan_for_rsdp(void) {
    uint32_t ebda = (*(uint16_t *)(uintptr_t)0x40E) << 4;
    if (ebda >= 0x80000 && ebda < 0xA0000) {
        for (uint8_t *p=(uint8_t*)(uintptr_t)ebda; p<(uint8_t*)(uintptr_t)(ebda+1024); p+=16)
            if (p[0]=='R'&&p[1]=='S'&&p[2]=='D'&&p[3]==' '&&
                p[4]=='P'&&p[5]=='T'&&p[6]=='R'&&p[7]==' '&&checksum_ok(p,20))
                return (rsdp_t*)p;
    }
    for (uint8_t *p=(uint8_t*)0xE0000; p<(uint8_t*)0x100000; p+=16)
        if (p[0]=='R'&&p[1]=='S'&&p[2]=='D'&&p[3]==' '&&
            p[4]=='P'&&p[5]=='T'&&p[6]=='R'&&p[7]==' '&&checksum_ok(p,20))
            return (rsdp_t*)p;
    return 0;
}

static sdt_header_t *find_table(rsdp_t *rsdp, const char sig[4]) {
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        sdt_header_t *x = (sdt_header_t*)(uintptr_t)rsdp->xsdt_address;
        uint32_t n = (x->length - sizeof(sdt_header_t)) / 8;
        uint64_t *ptrs = (uint64_t*)((uint8_t*)x + sizeof(sdt_header_t));
        for (uint32_t i=0;i<n;i++) {
            sdt_header_t *h = (sdt_header_t*)(uintptr_t)ptrs[i];
            if (sig4eq(h->signature, sig)) return h;
        }
    } else {
        sdt_header_t *r = (sdt_header_t*)(uintptr_t)rsdp->rsdt_address;
        uint32_t n = (r->length - sizeof(sdt_header_t)) / 4;
        uint32_t *ptrs = (uint32_t*)((uint8_t*)r + sizeof(sdt_header_t));
        for (uint32_t i=0;i<n;i++) {
            sdt_header_t *h = (sdt_header_t*)(uintptr_t)ptrs[i];
            if (sig4eq(h->signature, sig)) return h;
        }
    }
    return 0;
}

static void parse_madt(madt_t *madt) {
    g_acpi.lapic_phys = madt->lapic_address;
    uint8_t *p = (uint8_t*)madt + sizeof(madt_t);
    uint8_t *end = (uint8_t*)madt + madt->header.length;
    while (p < end) {
        madt_hdr_t *h = (madt_hdr_t*)p;
        if (!h->length) break;
        switch (h->type) {
        case 0: { madt_lapic_t *e=(madt_lapic_t*)p;
                  if ((e->flags&1)&&g_acpi.cpu_count<64)
                      g_acpi.lapic_ids[g_acpi.cpu_count++]=e->apic_id; break; }
        case 1: { madt_ioapic_t *e=(madt_ioapic_t*)p;
                  if (!g_acpi.ioapic_phys){g_acpi.ioapic_phys=e->address;g_acpi.ioapic_gsi_base=e->gsi_base;} break; }
        case 2: { if (g_acpi.override_count<ACPI_MAX_OVERRIDES) {
                      madt_override_t *e=(madt_override_t*)p;
                      acpi_override_t *ov=&g_acpi.overrides[g_acpi.override_count++];
                      ov->bus=e->bus; ov->source_irq=e->source; ov->gsi=e->gsi; ov->flags=e->flags; } break; }
        }
        p += h->length;
    }
}

int acpi_init(uint64_t rsdp_hint) {
    __builtin_memset(&g_acpi, 0, sizeof(g_acpi));
    rsdp_t *rsdp = rsdp_hint ? (rsdp_t*)(uintptr_t)rsdp_hint : scan_for_rsdp();
    if (!rsdp) { DPRINT("[ACPI] RSDP not found\n"); return -1; }
    DPRINT("[ACPI] RSDP revision=");
    DPRINT(rsdp->revision >= 2 ? "2 (XSDT)\n" : "1 (RSDT)\n");
    sdt_header_t *madt_hdr = find_table(rsdp, "APIC");
    if (!madt_hdr) { DPRINT("[ACPI] MADT not found\n"); return -1; }
    parse_madt((madt_t*)madt_hdr);
    DPRINT("[ACPI] LAPIC=");  DPRINT_HEX((uint32_t)g_acpi.lapic_phys);
    DPRINT(" IOAPIC=");       DPRINT_HEX((uint32_t)g_acpi.ioapic_phys);
    DPRINT(" CPUs=");         DPRINT_HEX(g_acpi.cpu_count); DPRINT("\n");
    return 0;
}

acpi_info_t *acpi_get_info(void) { return &g_acpi; }