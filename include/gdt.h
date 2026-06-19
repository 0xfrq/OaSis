#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

/* Selector constants */
#define KERNEL_CS   0x08
#define KERNEL_DS   0x10
#define USER_CS     0x1B          /* 0x18 | 0x03 = ring 3 code */
#define USER_DS     0x23          /* 0x20 | 0x03 = ring 3 data */
#define TSS_SEL     0x28

#endif
