#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct {
    uint16_t offset_lo;      // 16 bit bawah alamat handler
    uint16_t selector;       // Selector segmen kode kernel (0x08)
    uint8_t  reserved;       // Selalu 0
    uint8_t  type_attr;      // Tipe sama atribut (0x8E = trap gate)
    uint16_t offset_hi;      // 16 bit atas alamat handler
} __attribute__((packed)) IDTEntry;

typedef struct {
    uint16_t limit;          // Ukuran IDT dikurang 1
    uint32_t base;           // Alamat base IDT
} __attribute__((packed)) IDTPointer;

void idt_init(void);
void idt_set_entry(int num, uint32_t handler, uint16_t selector, uint8_t type_attr);

#endif
