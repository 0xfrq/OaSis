#include "paging.h"
#include "vga.h"
#include "string.h"

// Kernel page directory (must be 4KB aligned, at 0x1000)
__attribute__((aligned(0x1000)))
pde_t kernel_page_dir[PAGE_DIR_SIZE];

// Kernel page tables (1 table per 4MB of virtual space)
__attribute__((aligned(0x1000)))
static pte_t kernel_page_tables[10][PAGE_TABLE_SIZE];

static int page_table_index = 0;

void paging_init(void) {
    vga_print("[*] Initializing paging...\n");
    
    // Clear page directory
    for (int i = 0; i < PAGE_DIR_SIZE; i++) {
        kernel_page_dir[i] = 0;
    }
    
    // Identity map first 4MB (kernel code is here)
    pte_t *pt = kernel_page_tables[0];
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        pt[i] = (i * PAGE_SIZE) | PTE_PRESENT | PTE_WRITE;
    }
    kernel_page_dir[0] = ((uint32_t)pt) | PTE_PRESENT | PTE_WRITE;
    
    // Map kernel at 0xC0000000 (higher-half kernel)
    // This allows kernel code to use high addresses
    for (int i = 0; i < 4; i++) {
        pt = kernel_page_tables[i + 1];
        for (int j = 0; j < PAGE_TABLE_SIZE; j++) {
            pt[j] = ((i * PAGE_SIZE * PAGE_TABLE_SIZE) + (j * PAGE_SIZE)) | 
                    PTE_PRESENT | PTE_WRITE;
        }
        kernel_page_dir[0xC00 + i] = ((uint32_t)pt) | PTE_PRESENT | PTE_WRITE;
    }
    
    vga_print("[+] Paging structures initialized\n");
}

void paging_enable(void) {
    vga_print("[*] Enabling paging...\n");
    
    // Load page directory address into CR3
    uint32_t pd_addr = (uint32_t)kernel_page_dir;
    asm volatile("mov %0, %%cr3" : : "r"(pd_addr));
    
    // Enable paging bit in CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // PG bit
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    
    vga_print("[+] Paging enabled (CR0.PG = 1)\n");
}

uint32_t virt_to_phys(uint32_t virt) {
    uint32_t dir_index = virt >> 22;
    uint32_t table_index = (virt >> 12) & 0x3FF;
    uint32_t offset = virt & 0xFFF;
    
    pde_t pde = kernel_page_dir[dir_index];
    if (!(pde & PTE_PRESENT)) {
        return 0;  // Not mapped
    }
    
    pte_t *pt = (pte_t *)(pde & PAGE_MASK);
    pte_t pte = pt[table_index];
    if (!(pte & PTE_PRESENT)) {
        return 0;  // Not mapped
    }
    
    return (pte & PAGE_MASK) | offset;
}

void page_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_index = virt >> 22;
    uint32_t table_index = (virt >> 12) & 0x3FF;
    
    // Ensure page directory entry exists
    if (!(kernel_page_dir[dir_index] & PTE_PRESENT)) {
        if (page_table_index >= 10) {
            vga_print("ERROR: Out of page tables\n");
            return;
        }
        pte_t *pt = kernel_page_tables[page_table_index++];
        for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
            pt[i] = 0;
        }
        kernel_page_dir[dir_index] = ((uint32_t)pt) | PTE_PRESENT | PTE_WRITE;
    }
    
    // Get page table and map page
    pte_t *pt = (pte_t *)(kernel_page_dir[dir_index] & PAGE_MASK);
    pt[table_index] = (phys & PAGE_MASK) | flags | PTE_PRESENT;
}
