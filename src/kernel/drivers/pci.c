#include "pci.h"
#include "io.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)(dev & 0x1F) << 11) |
        ((uint32_t)(func & 0x07) << 8) |
        (reg & 0xFC);

    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t value) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)(dev & 0x1F) << 11) |
        ((uint32_t)(func & 0x07) << 8) |
        (reg & 0xFC);

    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func) {
    /* Read command register (offset 0x04) */
    uint32_t cmd = pci_config_read(bus, dev, func, 0x04);
    /* Set bit 2 (Bus Master) and bit 0 (I/O Space) */
    cmd |= (1 << 2) | (1 << 0);
    pci_config_write(bus, dev, func, 0x04, cmd);
}

static void pci_read_device(pci_device_t *d, uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t id = pci_config_read(bus, dev, func, 0);
    uint32_t class_reg = pci_config_read(bus, dev, func, 8);
    uint32_t irq_reg   = pci_config_read(bus, dev, func, 0x3C);

    d->bus        = bus;
    d->dev        = dev;
    d->func       = func;
    d->vendor_id  = (uint16_t)(id & 0xFFFF);
    d->device_id  = (uint16_t)((id >> 16) & 0xFFFF);
    d->class_code = (uint8_t)((class_reg >> 24) & 0xFF);
    d->subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
    d->prog_if    = (uint8_t)((class_reg >> 8) & 0xFF);
    d->irq        = (uint8_t)(irq_reg & 0xFF);
    d->present    = 1;

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_config_read(bus, dev, func, 0x10 + i * 4);
    }
}

static void pci_scan_device(uint8_t bus, uint8_t dev) {
    uint32_t id = pci_config_read(bus, dev, 0, 0);
    if ((uint16_t)(id & 0xFFFF) == PCI_VENDOR_INVALID)
        return;

    /* Function 0 */
    if (pci_device_count < MAX_PCI_DEVICES) {
        pci_read_device(&pci_devices[pci_device_count], bus, dev, 0);
        pci_device_count++;
    }

    /* Check multi-function */
    uint32_t header = pci_config_read(bus, dev, 0, 0x0C);
    if ((header >> 16) & PCI_HEADER_TYPE_MULTIFUNC) {
        for (int func = 1; func < 8; func++) {
            uint32_t fid = pci_config_read(bus, dev, func, 0);
            if ((uint16_t)(fid & 0xFFFF) != PCI_VENDOR_INVALID) {
                if (pci_device_count < MAX_PCI_DEVICES) {
                    pci_read_device(&pci_devices[pci_device_count], bus, dev, func);
                    pci_device_count++;
                }
            }
        }
    }
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
        case 0x08: return "Generic System";
        case 0x09: return "Input";
        case 0x0A: return "Docking";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intelligent I/O";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "Signal Processing";
        default:   return "Unknown";
    }
}

void pci_init(void) {
    vga_print("[*] Scanning PCI bus...\n");

    pci_device_count = 0;

    for (int dev = 0; dev < 32; dev++) {
        pci_scan_device(0, dev);
    }

    char buf[16];
    vga_print("[+] PCI devices found: ");
    itoa(pci_device_count, buf, 10);
    vga_print(buf);
    vga_print("\n");

    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t *d = &pci_devices[i];

        vga_print("  ");
        itoa((int)d->bus, buf, 10);
        vga_print(buf);
        vga_print(":");
        itoa((int)d->dev, buf, 10);
        vga_print(buf);
        vga_print(":");
        itoa((int)d->func, buf, 10);
        vga_print(buf);

        vga_print("  ");
        /* vendor:device hex */
        itoa(d->vendor_id, buf, 16);
        if (d->vendor_id < 0x1000) vga_print("0");
        if (d->vendor_id < 0x100)  vga_print("0");
        if (d->vendor_id < 0x10)   vga_print("0");
        vga_print(buf);
        vga_print(":");
        itoa(d->device_id, buf, 16);
        if (d->device_id < 0x1000) vga_print("0");
        if (d->device_id < 0x100)  vga_print("0");
        if (d->device_id < 0x10)   vga_print("0");
        vga_print(buf);

        vga_print("  ");
        vga_print(pci_class_name(d->class_code));
        vga_print(" IRQ:");
        itoa((int)d->irq, buf, 10);
        vga_print(buf);

        vga_print("\n");
    }
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return i;
        }
    }
    return -1;
}

int pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass == subclass) {
            return i;
        }
    }
    return -1;
}

int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count)
        return NULL;
    return &pci_devices[index];
}
