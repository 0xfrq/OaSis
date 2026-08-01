#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

#define PCI_VENDOR_INVALID 0xFFFF

#define PCI_CLASS_MASS_STORAGE    0x01
#define PCI_CLASS_NETWORK         0x02
#define PCI_CLASS_DISPLAY         0x03
#define PCI_CLASS_BRIDGE          0x06

#define PCI_HEADER_TYPE_MULTIFUNC 0x80

#define MAX_PCI_DEVICES 32

typedef struct {
    uint8_t  bus;
    uint8_t  dev;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq;
    uint32_t bar[6];
    uint8_t  present;
} pci_device_t;

uint32_t pci_config_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
void     pci_config_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t value);
void     pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func);
int      pci_find_class(uint8_t class_code, uint8_t subclass);
int      pci_find_device(uint16_t vendor_id, uint16_t device_id);
int      pci_get_device_count(void);
pci_device_t *pci_get_device(int index);
void     pci_init(void);

#endif
