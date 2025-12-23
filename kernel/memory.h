#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define E820_MAX_ENTRIES 20
#define E820_USABLE 1
#define E820_RESERVED 2
#define E820_ACPI 3
#define E820_BAD 4

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
} e820_entry_t;

typedef struct {
    uint32_t count;
    e820_entry_t entries[E820_MAX_ENTRIES];
} e820_map_t;

extern e820_map_t e820_map;

void memory_init(void);
uint32_t memory_get_total_sable(void);
void memory_print_map(void);

#endif