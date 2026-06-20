# memory management

dokumentasi ini ngebahas gimana OaSis ngatur memory.

## daftar isi

- [physical memory manager](#physical-memory-manager)
- [bitmap allocation](#bitmap-allocation)
- [memory map](#memory-map)
- [api reference](#api-reference)

---

## physical memory manager

**pmm (physical memory manager)** ngatur physical memory (RAM) dalam bentuk page.

### konsep page

- 1 page = 4 KB (4096 bytes)
- memory dibagi jadi page-page
- alloc dan free per page

**kenapa 4 KB?**
- standard di x86
- match dengan paging granularity
- balance antara granularity dan overhead

## bitmap allocation

pmm pake **bitmap** buat track page mana yang free/used.

### struktur bitmap

```c
// 1 bit = 1 page
// bit 0 = page 0, bit 1 = page 1, dst
uint8_t memory_bitmap[BITMAP_SIZE];

// bit 0 = free, bit 1 = used
```

### contoh

```
bitmap: 01001100 00000000 ...
         ││││││││
         │││││││└─ page 7: free
         ││││││└── page 6: free
         │││││└─── page 5: used
         ││││└──── page 4: used
         │││└───── page 3: free
         ││└────── page 2: free
         │└─────── page 1: used
         └──────── page 0: free
```

### operasi

**alloc page:**
```c
void *pmm_alloc_page(void) {
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return (void *)(i * PAGE_SIZE);
        }
    }
    return NULL; // out of memory
}
```

**free page:**
```c
void pmm_free_page(void *addr) {
    uint32_t page = (uint32_t)addr / PAGE_SIZE;
    bitmap_clear(page);
}
```

## memory map

OaSis pake flat memory model:

```
address range          description
─────────────────────────────────────────
0x00000000-0x000FFFFF  reserved (1 MB)
  0x00000000-0x000003FF  real mode ivt
  0x00000400-0x000004FF  bios data area
  0x00000500-0x00007BFF  free
  0x00007C00-0x00007DFF  bootloader
  0x00007E00-0x0007FFFF  free
  0x00080000-0x0009FFFF  extended bios data area
  0x000A0000-0x000BFFFF  vga buffer
  0x000C0000-0x000C7FFF  video bios
  0x000C8000-0x000EFFFF  expansion
  0x000F0000-0x000FFFFF  system bios

0x00100000-0x002FFFFF  kernel (2 MB)
  0x00100000-0x001FFFFF  kernel code + data
  0x00200000-0x002FFFFF  kernel stack

0x00300000-onwards     free memory
```

### reserved regions

**vga buffer: 0xB8000-0xBFFFF**
- 32 KB buat text mode
- 80x25 characters
- 2 bytes per character (char + attribute)

**kernel: 0x100000-0x2FFFFF**
- kernel di-load di 1 MB (standard)
- 2 MB cukup buat kernel code + data + stack

## api reference

### inisialisasi

```c
void pmm_init(void);
```

inisialisasi pmm. dipanggil sekali di startup.

**yang dilakuin:**
1. detect total memory (dari multiboot info)
2. inisialisasi bitmap (semua page marked as free)
3. mark reserved regions sebagai used

### alloc page

```c
void *pmm_alloc_page(void);
```

alloc satu page (4 KB).

**return:**
- pointer ke page (success)
- `NULL` (out of memory)

**contoh:**
```c
void *page = pmm_alloc_page();
if (page == NULL) {
    // out of memory!
}
```

### free page

```c
void pmm_free_page(void *addr);
```

free satu page yang udah di-alloc.

**parameter:**
- `addr`: pointer ke page yang mau di-free

**contoh:**
```c
pmm_free_page(page);
```

### get free pages

```c
uint32_t pmm_get_free_pages(void);
```

dapetin jumlah page yang masih free.

**return:** jumlah free pages

### get total pages

```c
uint32_t pmm_get_total_pages(void);
```

dapetin total page di system.

**return:** total pages

### get used pages

```c
uint32_t pmm_get_used_pages(void);
```

dapetin jumlah page yang lagi dipake.

**return:** used pages

---

## troubleshooting

### out of memory

kalo `pmm_alloc_page()` return `NULL`:
- cek memory leak (page di-alloc tapi gak di-free)
- cek total memory yang available
- reduce memory usage

### memory corruption

kalo ada weird behavior:
- cek double free (free page yang sama 2x)
- cek buffer overflow (nulis di luar allocated page)
- cek use-after-free (pake page yang udah di-free)

---

**kembali ke:** [kernel →](readme.md)
