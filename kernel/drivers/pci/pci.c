#include "pci.h"
#include "../../graphics/text.h"
#include "../../stdint.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    return (dword >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    return (dword >> ((offset & 3) * 8)) & 0xFF;
}

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    int shift = (offset & 2) * 8;
    dword = (dword & ~(0xFFFF << shift)) | (value << shift);
    pci_config_write_dword(bus, slot, func, offset, dword);
}

void pci_config_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    int shift = (offset & 3) * 8;
    dword = (dword & ~(0xFF << shift)) | (value << shift);
    pci_config_write_dword(bus, slot, func, offset, dword);
}

static void _hex_to_str(uint32_t val, char *buf, int width) {
    int i = width - 1;
    while (i >= 0) {
        buf[i--] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
}

void pci_init(void) {
    device_count = 0;
}

int pci_enumerate(void) {
    DPRINT("\n");
    DPRINT("========== PCI FIND START ==========\n");
    
    device_count = 0;
    int found = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t vendor_id = pci_config_read_word(bus, slot, func, 0);

                if (vendor_id == 0xFFFF) continue;
                
                if (device_count >= PCI_MAX_DEVICES) {
                    DPRINT("PCI: Max devices reached!\n");
                    goto done;
                }

                found++;
                pci_device_t *dev = &devices[device_count++];
                dev->bus = bus;
                dev->slot = slot;
                dev->function = func;

                dev->vendor_id = vendor_id;
                dev->device_id = pci_config_read_word(bus, slot, func, 2);
                dev->class_code = pci_config_read_byte(bus, slot, func, 0x0B);
                dev->subclass = pci_config_read_byte(bus, slot, func, 0x0A);
                dev->prog_if = pci_config_read_byte(bus, slot, func, 0x09);
                dev->revision_id = pci_config_read_byte(bus, slot, func, 0x08);

                dev->bar0 = pci_config_read_dword(bus, slot, func, 0x10);
                dev->bar1 = pci_config_read_dword(bus, slot, func, 0x14);
                dev->bar2 = pci_config_read_dword(bus, slot, func, 0x18);
                dev->bar3 = pci_config_read_dword(bus, slot, func, 0x1C);
                dev->bar4 = pci_config_read_dword(bus, slot, func, 0x20);
                dev->bar5 = pci_config_read_dword(bus, slot, func, 0x24);

                dev->subsystem_vendor_id = pci_config_read_word(bus, slot, func, 0x2C);
                dev->subsystem_id = pci_config_read_word(bus, slot, func, 0x2E);

                dev->irq_line = pci_config_read_byte(bus, slot, func, 0x3C);

                char line[96];
                int len = 0;
                
                const char *p = "Device ";
                while (*p) line[len++] = *p++;

                char num[4];
                _hex_to_str(found, num, 1);
                line[len++] = num[0];
                
                p = ": [";
                while (*p) line[len++] = *p++;

                _hex_to_str(dev->bus, num, 2);
                line[len++] = num[0];
                line[len++] = num[1];
                
                p = ":";
                while (*p) line[len++] = *p++;

                _hex_to_str(dev->slot, num, 2);
                line[len++] = num[0];
                line[len++] = num[1];
                
                p = ":";
                while (*p) line[len++] = *p++;

                line[len++] = "0123456789ABCDEF"[dev->function & 0xF];
                
                p = "] ";
                while (*p) line[len++] = *p++;

                _hex_to_str(dev->vendor_id, num, 4);
                for (int i = 0; i < 4; i++) line[len++] = num[i];
                
                line[len++] = ':';
                
                _hex_to_str(dev->device_id, num, 4);
                for (int i = 0; i < 4; i++) line[len++] = num[i];
                
                line[len++] = '\n';
                line[len] = 0;
                
                DPRINT(line);

                DPRINT("  Class: ");
                DPRINT_HEX(dev->class_code);
                DPRINT(" Subclass: ");
                DPRINT_HEX(dev->subclass);
                DPRINT("\n");
                
                DPRINT("  BAR0: ");
                DPRINT_HEX(dev->bar0);
                DPRINT("  BAR1: ");
                DPRINT_HEX(dev->bar1);
                DPRINT("\n");
                
                DPRINT("  IRQ: ");
                char irq[4];
                _hex_to_str(dev->irq_line, irq, 2);
                DPRINT(irq);
                DPRINT("\n");
                DPRINT("\n");
            }
        }
    }

done:
    DPRINT("========== PCI FIND END ==========\n");
    char summary[64];
    int slen = 0;
    
    const char *sp = "Total devices found: ";
    while (*sp) summary[slen++] = *sp++;

    if (found >= 10) summary[slen++] = '0' + (found / 10);
    summary[slen++] = '0' + (found % 10);
    
    sp = "\n\n";
    while (*sp) summary[slen++] = *sp++;
    summary[slen] = 0;
    
    DPRINT(summary);
    
    return device_count;
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor_id && devices[i].device_id == device_id)
            return &devices[i];
    }
    return NULL;
}

pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= device_count) return NULL;
    return &devices[index];
}

int pci_get_device_count(void) {
    return device_count;
}

static const char *pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Communication";
        case 0x08: return "System Peripheral";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intelligent IO";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "Data Acquisition";
        case 0xFF: return "Other";
        default:   return "Unknown";
    }
}

void pci_print_devices(void) {
    text_puts(0, 0, "=== PCI Devices ===", 0xffffff, 0, 1);
    int y = 20;
    for (int i = 0; i < device_count; i++) {
        pci_device_t *dev = &devices[i];
        char status[64];
        int si = 0;
        const char *p = dev->vendor_id;
        y += 16;
    }
}