#include "pmm.h"
#include "vga.h"
#include "string.h"

#define PAGE_SIZE 0x1000
#define BITMAP_SIZE 1024 * 1024  // 1M bytes = 8M pages = 32GB addressable

static uint8_t page_bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t free_pages = 0;

void pmm_init(uint32_t total_memory) {
    vga_print("[*] Initializing physical memory manager...\n");
    
    // Clear bitmap
    for (int i = 0; i < BITMAP_SIZE; i++) {
        page_bitmap[i] = 0xFF;  // Mark all as used initially
    }
    
    total_pages = total_memory / PAGE_SIZE;
    
    // Mark usable pages as free
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        if (byte < BITMAP_SIZE) {
            page_bitmap[byte] &= ~(1 << bit);
            free_pages++;
        }
    }
    
    // Mark kernel space (0x0 - 0x100000) as used
    for (uint32_t i = 0; i < (0x100000 / PAGE_SIZE); i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        page_bitmap[byte] |= (1 << bit);
        free_pages--;
    }
    
    char buf[16];
    vga_print("[+] PMM initialized: ");
    itoa(free_pages, buf, 10);
    vga_print(buf);
    vga_print(" free pages (");
    itoa(free_pages * PAGE_SIZE / 1024 / 1024, buf, 10);
    vga_print(buf);
    vga_print("MB)\n");
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        if (byte >= BITMAP_SIZE) break;
        
        if (!(page_bitmap[byte] & (1 << bit))) {
            page_bitmap[byte] |= (1 << bit);
            free_pages--;
            return i * PAGE_SIZE;
        }
    }
    vga_print("WARNING: No free pages\n");
    return 0;
}

void pmm_free_page(uint32_t phys) {
    uint32_t page = phys / PAGE_SIZE;
    uint32_t byte = page / 8;
    uint32_t bit = page % 8;
    
    if (byte < BITMAP_SIZE) {
        page_bitmap[byte] &= ~(1 << bit);
        free_pages++;
    }
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}
