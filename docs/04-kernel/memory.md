---
layout: default
title: Memory Management
---

# Memory Management

## Physical Memory Manager (PMM)

The PMM uses a simple **bitmap allocator**.

### Algorithm
- A bitmap of size `BITMAP_SIZE = 1MB` covers `8M` pages (each bit = 1 page).
- Total addressable memory: `8M * 4KB = 32GB`.
- `pmm_init(total_memory)`: initially marks all pages as used (0xFF), then iterates from page 0 to `total_pages`, clearing each bit. Finally marks the first 1MB (kernel area) and the kernel binary range (0x100000 to `_end`) as used.
- `pmm_alloc_page()`: linear scan of the bitmap for the first cleared bit, sets it, decrements `free_pages`, returns the physical address (`page_index * 4096`).
- `pmm_free_page(phys)`: clears the corresponding bit, increments `free_pages`.

### Code Path
```text
malloc() -> kmalloc() -> pmm_alloc_page() -> bitmap scan -> physical page
```

## Paging

OaSis uses **4KB page tables** with a 2-level hierarchy.

### Page Directory and Page Tables
- `kernel_page_dir[1024]` (4KB aligned) — one PDE per 4MB of virtual address space.
- `kernel_page_tables[10][1024]` — static pool of page tables (expanded to 128 entries).
- Each PTE maps 4KB of virtual to physical memory.

### Initialization (`paging_init`)
1. Clear all PDEs.
2. PDE 0 (0x00000000-0x003FFFFF): identity map first 4MB.
3. PDE 0xC00-0xC03 (0xC0000000-0xC0400000): higher-half kernel mapping (recursive mapping of same physical pages).

### `page_map(virt, phys, flags)`
1. Compute `dir_index = virt >> 22`, `table_index = (virt >> 12) & 0x3FF`.
2. If PDE is not present, allocate a new page table from the pool.
3. Set `PT[table_index] = phys | flags | PTE_PRESENT`.
4. Propagate USER flag to PDE if needed.

### Process Isolation
`paging_create_user_dir()` creates a **dedicated page directory per user task**:
1. Allocate physical page for new PD via `pmm_alloc_page()`.
2. Map PD temporarily at `0x300000`.
3. Clone each PDE from `kernel_page_dir`:
   - Kernel PDEs (0, 0xC00-0xC03): copy PTEs **without** `PTE_USER`.
   - User PDEs (others): copy PTEs **with** `PTE_USER`.
4. Return physical address of new PD (stored in `task->context.cr3`).
5. `paging_switch_dir()` loads CR3, switching to the new page table.

## Kernel Heap (kmalloc)

### Block Structure
```text
[heap_block_t header: 16 bytes] [payload: N bytes]
```

`heap_block_t`:
```c
uint32_t size;           // block total size (header + payload)
uint16_t magic;          // MAGIC_FREE (0xF4EE) or MAGIC_USED (0x1CED)
uint16_t flags;          // bit 0: free (1) or used (0)
heap_block_t *next;       // next free block (free list)
heap_block_t *prev;       // prev free block (free list)
```

### Allocation (`kmalloc`)
1. Align requested size to 8 bytes, add header size.
2. First-fit scan of the free list.
3. If block is large enough to split (> `needed + BLOCK_MIN`):
   - Split: create new free block from remaining space.
   - Insert new block into free list at sorted position.
4. Remove block from free list, mark as USED.
5. If no free block found, expand heap by 4KB (or more), coalesce with last free block if adjacent.

### Deallocation (`kfree`)
1. Validate pointer: check magic number.
2. Mark block as FREE, add to free list (sorted by address).
3. Coalesce with adjacent free blocks (next and previous in address order).

### Heap Expansion
- Heap starts at `0x02000000` with 64KB initial size.
- When full, allocates physical pages via PMM and maps them at the heap boundary.
- Maximum heap size: 16MB.
