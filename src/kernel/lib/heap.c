/*
 * heap.c - Kernel heap allocator (free-list based)
 *
 * Implementasi malloc/free untuk OaSis kernel.
 * Pakai first-fit free list dengan splitting dan coalescing.
 *
 * Heap region: 0x02000000 - 0x03000000 (16MB)
 */

#include "heap.h"
#include "pmm.h"
#include "paging.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

/* ====== Konfigurasi ====== */

/* Virtual address heap dimulai di sini (di atas kernel binary, di bawah CODE_VIRT) */
#define HEAP_BASE     0x02000000
/* Ukuran maksimal heap (16MB) */
#define HEAP_MAX_SIZE 0x01000000
/* Ukuran minimal satu block (header + minimal payload) */
#define BLOCK_MIN   (sizeof(heap_block_t) + 8)
/* Alignment payload */
#define ALIGNMENT   8
/* Magic numbers buat deteksi corruption */
#define MAGIC_FREE  0xF4EE
#define MAGIC_USED  0x1CED

/* ====== Struktur Block Header ====== */

typedef struct heap_block {
    uint32_t size;            /* Total ukuran block (termasuk header), di-align ke 8 */
    uint16_t magic;           /* MAGIC_FREE atau MAGIC_USED */
    uint16_t flags;           /* bit 0: free (1=free, 0=used) */
    struct heap_block *next;  /* Next free block (cuma kepake kalo free) */
    struct heap_block *prev;  /* Prev free block (cuma kepake kalo free) */
} heap_block_t;

/* ====== State ====== */

/* Head of free list */
static heap_block_t *free_head = NULL;
/* Total heap pages yang sudah di-commit (dapat page fisik) */
static uint32_t committed_pages = 0;
/* Inisialisasi udah dilakukan? */
static int heap_initialized = 0;

/* ====== Helper ====== */

/* Align size ke ALIGNMENT */
static inline uint32_t align_up(uint32_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/* Dapatkan address setelah header */
static inline void *block_payload(heap_block_t *block) {
    return (void *)((uint8_t *)block + sizeof(heap_block_t));
}

/* Dapatkan block header dari payload pointer */
static inline heap_block_t *payload_block(void *ptr) {
    return (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
}

/* Expand heap dengan mapping page fisik baru */
static int expand_heap(uint32_t min_bytes) {
    uint32_t needed_pages = (min_bytes + 0xFFF) / 0x1000;
    if (needed_pages < 1) needed_pages = 1;

    uint32_t curr_addr = HEAP_BASE + committed_pages * 0x1000;
    uint32_t max_addr  = HEAP_BASE + HEAP_MAX_SIZE;

    for (uint32_t i = 0; i < needed_pages; i++) {
        if (curr_addr + i * 0x1000 >= max_addr) {
            vga_print("heap: out of virtual address space!\n");
            return -1;
        }
        uint32_t phys = pmm_alloc_page();
        if (phys == 0) {
            vga_print("heap: out of physical pages!\n");
            return -1;
        }
        page_map(curr_addr + i * 0x1000, phys, PTE_PRESENT | PTE_WRITE);
    }

    /* Invalidate TLB for the new range */
    asm volatile("invlpg (%0)" : : "r"(curr_addr) : "memory");

    committed_pages += needed_pages;
    return (int)needed_pages;
}

/* ====== Free List Management ====== */

/* Hapus block dari free list */
static void freelist_remove(heap_block_t *block) {
    if (block->prev)
        block->prev->next = block->next;
    else
        free_head = block->next;

    if (block->next)
        block->next->prev = block->prev;

    block->next = NULL;
    block->prev = NULL;
}

/* Tambah block ke free list (insert sorted by address) */
static void freelist_add(heap_block_t *block) {
    block->magic = MAGIC_FREE;
    block->flags = 1;  /* free */

    if (!free_head || block < free_head) {
        /* Insert di depan */
        block->next = free_head;
        block->prev = NULL;
        if (free_head) free_head->prev = block;
        free_head = block;
        return;
    }

    /* Cari posisi yang tepat */
    heap_block_t *cur = free_head;
    while (cur->next && cur->next < block) {
        cur = cur->next;
    }
    block->next = cur->next;
    block->prev = cur;
    if (cur->next) cur->next->prev = block;
    cur->next = block;
}

/* Coalesce adjacent free blocks */
static void freelist_coalesce(heap_block_t *block) {
    /* Coalesce dengan next block kalo dia free dan adjacent */
    if (block->next) {
        heap_block_t *next = (heap_block_t *)((uint8_t *)block + block->size);
        /* Cek kalo next block beneran ada di free list dan adjacent */
        heap_block_t *cur = free_head;
        while (cur) {
            if (cur == next && cur->flags) {
                /* Gabung */
                freelist_remove(cur);
                block->size += cur->size;
                break;
            }
            cur = cur->next;
        }
    }

    /* Coalesce dengan prev block kalo dia free dan adjacent */
    if (block->prev) {
        heap_block_t *prev = (heap_block_t *)((uint8_t *)block->prev);
        if ((uint8_t *)prev + prev->size == (uint8_t *)block && prev->flags) {
            freelist_remove(block);
            prev->size += block->size;
            block = prev;
        }
    }
}

/* ====== Inisialisasi Heap ====== */

static void heap_init(void) {
    if (heap_initialized) return;

    /* Alokasi page pertama */
    int npages = expand_heap(0x10000);  /* 64KB initial heap */
    if (npages < 0) {
        vga_print("heap: gagal initialisasi!\n");
        return;
    }

    uint32_t initial_size = (uint32_t)npages * 0x1000;

    /* Bikin block raksasa pertama */
    heap_block_t *first = (heap_block_t *)HEAP_BASE;
    first->size  = initial_size;
    first->magic = MAGIC_FREE;
    first->flags = 1;
    first->next  = NULL;
    first->prev  = NULL;

    free_head = first;
    heap_initialized = 1;

    vga_print("[*] Kernel heap initialized at 0x02000000\n");
}

/* ====== malloc (kmalloc) ====== */

void *kmalloc(uint32_t size) {
    if (!heap_initialized) heap_init();

    if (size == 0) return NULL;

    /* Minimal allocation */
    if (size < 8) size = 8;

    /* Total yang harus di-allocate (header + payload + alignment) */
    uint32_t needed = align_up(sizeof(heap_block_t) + size);

    /* First-fit search */
    heap_block_t *block = free_head;
    while (block) {
        if (!block->flags) {
            block = block->next;
            continue;
        }

        if (block->size >= needed) {
            /* Found! */
            if (block->size >= needed + BLOCK_MIN) {
                /* Split: block cukup gede, potong jadi dua */
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + needed);
                new_block->size  = block->size - needed;
                new_block->magic = MAGIC_FREE;
                new_block->flags = 1;

                /* Masukin new block ke free list */
                freelist_remove(block);
                block->size = needed;
                freelist_add(new_block);
                freelist_add(block);
            }

            /* Tandai sebagai used */
            freelist_remove(block);
            block->magic = MAGIC_USED;
            block->flags = 0;

            return block_payload(block);
        }
        block = block->next;
    }

    /* Not found: expand heap */
    uint32_t expand_size = needed > 0x1000 ? align_up(needed) : 0x1000;
    uint32_t old_committed = committed_pages; /* simpan sebelum expand */
    if (expand_heap(expand_size) < 0) {
        return NULL;
    }

    /* Buat block baru di halaman yang baru di-map */
    block = (heap_block_t *)(HEAP_BASE + old_committed * 0x1000);

    /* Coalesce dengan free block sebelumnya */
    if (free_head) {
        heap_block_t *last = free_head;
        while (last->next) last = last->next;
        uint32_t last_end = (uint32_t)last + last->size;
        if (last_end == (uint32_t)block && last->flags) {
            /* Expand block terakhir */
            last->size += expand_size;
            block = last;

            /* Coba split lagi dengan needed */
            if (block->size >= needed + BLOCK_MIN) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + needed);
                new_block->size  = block->size - needed;
                new_block->magic = MAGIC_FREE;
                new_block->flags = 1;
                freelist_remove(block);
                block->size = needed;
                freelist_add(new_block);
                freelist_add(block);
            }

            freelist_remove(block);
            block->magic = MAGIC_USED;
            block->flags = 0;
            return block_payload(block);
        }
    }

    /* Fallback: bikin block baru di halaman baru */
    block->size  = expand_size;
    block->magic = MAGIC_USED;
    block->flags = 0;
    block->next  = NULL;
    block->prev  = NULL;
    return block_payload(block);
}

/* ====== free (kfree) ====== */

void kfree(void *ptr) {
    if (!ptr) return;
    if (!heap_initialized) return;

    heap_block_t *block = payload_block(ptr);

    /* Validation */
    if (block->magic != MAGIC_USED) {
        /* Double free or invalid pointer */
        return;
    }

    block->magic = MAGIC_FREE;
    block->flags = 1;

    /* Masukin ke free list */
    freelist_add(block);

    /* Coalesce adjacent free blocks */
    freelist_coalesce(block);
}

/* ====== Debug ====== */

void heap_dump(void) {
    vga_print("=== HEAP DUMP ===\n");
    char buf[32];

    vga_print("Committed pages: ");
    itoa(committed_pages, buf, 10);
    vga_print(buf);
    vga_print("\n");

    /* Walk all blocks (free and used) */
    uint32_t addr = HEAP_BASE;
    uint32_t end  = HEAP_BASE + committed_pages * 0x1000;

    while (addr < end) {
        heap_block_t *block = (heap_block_t *)addr;
        if (block->size == 0 || block->size > HEAP_MAX_SIZE)
            break;

        vga_print(block->flags ? "  [FREE]  " : "  [USED]  ");
        itoa(block->size, buf, 10);
        vga_print(buf);
        vga_print(" bytes");
        if (block->flags == 0) {
            vga_print("  magic=");
            itoa(block->magic, buf, 16);
            vga_print(buf);
        }
        vga_print("\n");

        addr += block->size;
    }
}

/* ====== Utility ====== */

void *kcalloc(uint32_t num, uint32_t size) {
    uint32_t total = num * size;
    void *ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *krealloc(void *ptr, uint32_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    heap_block_t *block = payload_block(ptr);
    /* Bisa ngecek kalo block selanjutnya free dan adjacent buat expand-in-place */
    /* Simplify: alloc baru, copy, free lama */
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    uint32_t old_payload = block->size - sizeof(heap_block_t);
    uint32_t copy_size = old_payload < new_size ? old_payload : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    return new_ptr;
}
