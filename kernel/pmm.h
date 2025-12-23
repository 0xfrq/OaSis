#ifndef PMM_H
#define PMM_H

#include <stdint.h>

void pmm_init(uint32_t total_memory);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t phys);
uint32_t pmm_get_free_page(void);

#endif