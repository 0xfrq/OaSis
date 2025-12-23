#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct {
    uint16_t offset_lo;      // Handler address low 16 bits
    uint16_t selector;       // Kernel code segment selector (0x08)
    uint8_t  reserved;       // Always 0
    uint8_t  type_attr;      // Type and attributes (0x8E = trap gate)
    uint16_t offset_hi;      // Handler address high 16 bits
} __attribute__((packed)) IDTEntry;

typedef struct {
    uint16_t limit;          // Size of IDT - 1
    uint32_t base;           // Base address of IDT
} __attribute__((packed)) IDTPointer;

void idt_init(void);
void idt_set_entry(int num, uint32_t handler, uint16_t selector, uint8_t type_attr);

#endif
