#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 0x1000                    // 4KB
#define PAGE_MASK 0xFFFFF000               // 12 bit bawah itu offset
#define PAGE_DIR_SIZE 1024                 // 1024 entri
#define PAGE_TABLE_SIZE 1024               // 1024 entri

#define PTE_PRESENT 0x00000001
#define PTE_WRITE   0x00000002
#define PTE_USER    0x00000004
#define PTE_PWRT    0x00000008
#define PTE_PCACHE  0x00000010
#define PTE_ACCESSED 0x00000020
#define PTE_DIRTY   0x00000040
#define PTE_GLOBAL  0x00000100

typedef uint32_t pde_t;
typedef uint32_t pte_t;

extern pde_t kernel_page_dir[PAGE_DIR_SIZE];

void paging_init(void);
void paging_enable(void);
uint32_t virt_to_phys(uint32_t virt);
void page_map(uint32_t virt, uint32_t phys, uint32_t flags);
/* Process isolation: clone page dir untuk user task */
pde_t *paging_create_user_dir(void);
void paging_switch_dir(pde_t *dir);

#endif
