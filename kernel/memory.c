#include "memory.h"
#include "vga.h"
#include "string.h"

e820_map_t e820_map = {0};

void memory_init(void) {
    e820_map.count = 2;

    e820_map.entries[0].base = 0x00000000;
    e820_map.entries[0].length = 0x00100000;
    e820_map.entries[0].type = E820_RESERVED;

    e820_map.entries[1].base = 0x00100000;
    e820_map.entries[1].length = 0x7FE00000;
    e820_map.entries[0].type = E820_USABLE;
}

uint32_t memory_get_total_usable(void) {
    uint32_t total = 0;
    for (int i = 0; i < e820_map.count; i++) {
        if (e820_map.entries[i].type == E820_USABLE) {
            total += e820_map.entries[i].length;
        }
    }
    return total;
}

void memory_print_map(void) {
    vga_print("Memory Map (e820):\n");
    for(int i=0; i<e820_map.count; i++) {
        e820_entry_t *entry = &e820_map.entries[i];
        
        vga_print(" [");
        char buf[16];
        itoa(i, buf, 10);
        vga_print(buf);
        vga_print("] base: 0x");
        itoa((uint32_t)(entry->base >> 32), buf, 16);
        vga_print(buf);

        vga_print(" Length: 0x");
        itoa((uint32_t)(entry->length >> 32), buf, 16);
        vga_print(buf);
        itoa((uint32_t)entry->length, buf, 16);
        vga_print(buf);

        vga_print(" Type: ");
        itoa(entry->type, buf, 10);
        vga_print(buf);
        vga_print("\n");
    }
}