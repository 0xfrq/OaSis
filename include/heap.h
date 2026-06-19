#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

/*
 * heap.h - Kernel heap allocator (free-list based)
 *
 * Fungsi:
 *   kmalloc(size)    - alokasi memori
 *   kfree(ptr)       - free memori
 *   kcalloc(n, size) - alokasi + zero-initialize
 *   krealloc(ptr, s) - realloc
 *   heap_dump()      - debug: print state heap
 */

void  *kmalloc(uint32_t size);
void   kfree(void *ptr);
void  *kcalloc(uint32_t num, uint32_t size);
void  *krealloc(void *ptr, uint32_t new_size);
void   heap_dump(void);

#endif
