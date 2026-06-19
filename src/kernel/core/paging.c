#include "paging.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include <stddef.h>

// Kernel page directory (must be 4KB aligned, at 0x1000)
__attribute__((aligned(0x1000)))
pde_t kernel_page_dir[PAGE_DIR_SIZE];

// Kernel page tables (1 table per 4MB of virtual space)
__attribute__((aligned(0x1000)))
static pte_t kernel_page_tables[128][PAGE_TABLE_SIZE];

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

    page_table_index = 5; // index 0,1,2,3,4 sudah dipake

    vga_print("[+] Paging structures initialized\n");
}

void paging_enable(void) {
    vga_print("[*] Enabling paging...\n");

    // Load PHYSICAL address of page directory into CR3
    // CR3 requires a physical address, not virtual
    uint32_t pd_phys = (uint32_t)kernel_page_dir;  // Works with identity mapping enabled
    asm volatile("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
    
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
        if (page_table_index >= 32) {
            vga_print("ERROR: Out of page tables\n");
            return;
        }
        pte_t *pt = kernel_page_tables[page_table_index++];
        for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
            pt[i] = 0;
        }
        kernel_page_dir[dir_index] = ((uint32_t)pt) | PTE_PRESENT | PTE_WRITE;
        /* Propagate User bit to PDE kalo ada di flags */
        if (flags & PTE_USER) {
            kernel_page_dir[dir_index] |= PTE_USER;
        }
    } else {
        /* Pastiin PDE punya USER bit kalo page ini user-mode */
        if (flags & PTE_USER) {
            kernel_page_dir[dir_index] |= PTE_USER;
        }
    }

    // Get page table and map page
    pte_t *pt = (pte_t *)(kernel_page_dir[dir_index] & PAGE_MASK);
    pt[table_index] = (phys & PAGE_MASK) | flags | PTE_PRESENT;
}


/* ====== Process Isolation ====== */

/* Kernel dapat page table (alokasi dari kernel_page_tables) */
static pte_t *paging_alloc_pt(void) {
    if (page_table_index >= 32) {
        vga_print("ERROR: Out of kernel page tables\n");
        return NULL;
    }
    pte_t *pt = kernel_page_tables[page_table_index++];
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) pt[i] = 0;
    /* Zero the memory */
    return pt;
}

/* Clone kernel page directory untuk user task.
 * Copy semua entri dari kernel_page_dir, tapi:
 * - Identity map (PDE 0) STRIP PTE_USER (user gak boleh akses 0-4MB)
 * - Higher-half (PDE 0xC00-0xC03) STRIP PTE_USER
 * - Map user pages (0x800000, 0xF00000, 0x01000000) WITH PTE_USER
 *
 * Returns pointer ke page directory fisik (aligned 4KB),
 * atau NULL kalo gagal.
 */
pde_t *paging_create_user_dir(void) {
    /* Alokasi page directory PHYSICAL */
    uint32_t pd_phys = pmm_alloc_page();
    if (pd_phys == 0) return NULL;

    uint32_t pd_virt = 0x00300000;
    page_map(pd_virt, pd_phys, PTE_PRESENT | PTE_WRITE);
    asm volatile("invlpg (%0)" : : "r"(pd_virt) : "memory");

    pde_t *user_pd = (pde_t *)pd_virt;
    for (int i = 0; i < PAGE_DIR_SIZE; i++) user_pd[i] = 0;

    /* Temporary mapping area buat clone page tables */
    uint32_t tmp_virt = 0x00700000;

    /* Clone PDE dari kernel_page_dir */
    for (int i = 0; i < PAGE_DIR_SIZE; i++) {
        pde_t pde = kernel_page_dir[i];
        if (!(pde & PTE_PRESENT)) continue;

        uint32_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            vga_print("ERROR: OOM for PT in user dir\n");
            return NULL;
        }

        /* Map temporary */
        page_map(tmp_virt, pt_phys, PTE_PRESENT | PTE_WRITE);
        asm volatile("invlpg (%0)" : : "r"(tmp_virt) : "memory");

        /* Copy PTE entries */
        pte_t *old_pt = (pte_t *)(kernel_page_dir[i] & PAGE_MASK);
        pte_t *new_pt = (pte_t *)tmp_virt;
        int is_kernel_page = (i == 0) || (i >= 0xC00 && i < 0xD00);

        for (int j = 0; j < PAGE_TABLE_SIZE; j++) {
            if (old_pt[j] & PTE_PRESENT) {
                if (is_kernel_page) {
                    /* Kernel pages: strip USER flag */
                    new_pt[j] = old_pt[j] & ~PTE_USER;
                } else {
                    /* User pages: add USER flag */
                    new_pt[j] = old_pt[j] | PTE_USER;
                }
            } else {
                new_pt[j] = 0;
            }
        }

        /* Set PDE: kernel pages tanpa USER, user pages dengan USER */
        if (is_kernel_page) {
            user_pd[i] = pt_phys | PTE_PRESENT | PTE_WRITE;
        } else {
            user_pd[i] = pt_phys | PTE_PRESENT | PTE_WRITE | PTE_USER;
        }

        tmp_virt += 0x1000;
        if (tmp_virt >= 0x00800000) {
            vga_print("ERROR: Too many page tables\n");
            return NULL;
        }
    }

    vga_print("[+] User page dir created (all PDE cloned)\n");
    return (pde_t *)pd_phys;
}void paging_switch_dir(pde_t *dir) {
    uint32_t pd_phys;
    if (dir == NULL) {
        pd_phys = (uint32_t)kernel_page_dir;
    } else {
        pd_phys = (uint32_t)dir;
    }
    asm volatile("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}
