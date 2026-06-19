#ifndef NULL
#define NULL ((void*)0)
#endif
#include "block.h"
#include "ata.h"
#include <stdint.h>

// implementasi memcpy sederhana
static void memcpy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// cache blok
static block_cache_entry_t block_cache[BLOCK_CACHE_SIZE];

// antrian request I/O (alokasi statis)
static io_request_t io_requests[IO_QUEUE_SIZE];
static int io_request_count = 0;

// inisialisasi layer block device
void block_init(void) {
    ata_init();

    // inisialisasi cache
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        block_cache[i].valid = 0;
        block_cache[i].dirty = 0;
        block_cache[i].ref_count = 0;
    }

    // inisialisasi antrian I/O
    block_queue_init();
}

// cari cache entry buat blok yang diminta, atau cari slot kosong
static block_cache_entry_t *find_cache_entry(uint32_t block_num) {
    // pertama, cari entry yang udah ada
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].block_num == block_num) {
            return &block_cache[i];
        }
    }

    // gak ketemu, cari slot kosong
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (!block_cache[i].valid) {
            return &block_cache[i];
        }
    }

    // gak ada slot kosong, pake LRU (sederhana: entry valid pertama)
    // kalo implementasi beneran, harusnya track access time
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].ref_count == 0) {
            // write back kalo dirty
            if (block_cache[i].dirty) {
                ata_write_sector(block_cache[i].block_num, block_cache[i].data);
                block_cache[i].dirty = 0;
            }
            block_cache[i].valid = 0;
            return &block_cache[i];
        }
    }

    // semua entry lagi dipake, return NULL (harusnya gak kejadian kalo ref counting bener)
    return NULL;
}

// baca blok, pake cache
int block_read(uint32_t block_num, uint8_t *buffer) {
    block_cache_entry_t *entry = find_cache_entry(block_num);

    if (!entry) {
        // cache penuh, baca langsung aja
        return ata_read_sector(block_num, buffer);
    }

    if (!entry->valid || entry->block_num != block_num) {
        // load dari disk
        if (ata_read_sector(block_num, entry->data) != 0) {
            return -1;
        }
        entry->block_num = block_num;
        entry->valid = 1;
        entry->dirty = 0;
    }

    entry->ref_count++;
    memcpy(buffer, entry->data, BLOCK_SIZE);
    entry->ref_count--;

    return 0;
}

// tulis blok, pake cache
int block_write(uint32_t block_num, const uint8_t *buffer) {
    block_cache_entry_t *entry = find_cache_entry(block_num);

    if (!entry) {
        // cache penuh, tulis langsung aja
        return ata_write_sector(block_num, buffer);
    }

    // copy data ke cache
    memcpy(entry->data, buffer, BLOCK_SIZE);
    entry->block_num = block_num;
    entry->valid = 1;
    entry->dirty = 1;
    entry->ref_count++;

    // buat write-through cache, langsung tulis ke disk juga
    int result = ata_write_sector(block_num, buffer);
    if (result == 0) {
        entry->dirty = 0; // hapus flag dirty
    }
    entry->ref_count--;

    return result;
}

// flush semua blok yang dirty ke disk
void block_flush(void) {
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].dirty) {
            ata_write_sector(block_cache[i].block_num, block_cache[i].data);
            block_cache[i].dirty = 0;
        }
    }
}

// inisialisasi antrian request I/O
void block_queue_init(void) {
    io_request_count = 0;
}

// tambah request ke antrian I/O
int block_queue_request(io_operation_t op, uint32_t block_num, uint8_t *buffer) {
    if (io_request_count >= IO_QUEUE_SIZE) return -1; // antrian penuh

    io_request_t *req = &io_requests[io_request_count++];
    req->operation = op;
    req->block_num = block_num;
    req->buffer = buffer;
    req->completed = 0;
    req->success = 0;

    return 0;
}

// proses request I/O yang pending
void block_process_queue(void) {
    for (int i = 0; i < io_request_count; i++) {
        io_request_t *req = &io_requests[i];
        if (req->completed) continue;

        if (req->operation == IO_READ) {
            req->success = (block_read(req->block_num, req->buffer) == 0);
        } else if (req->operation == IO_WRITE) {
            req->success = (block_write(req->block_num, req->buffer) == 0);
        }

        req->completed = 1;
    }
}

// ambil statistik cache
int block_get_cache_valid_count(void) {
    int count = 0;
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid) count++;
    }
    return count;
}

int block_get_cache_dirty_count(void) {
    int count = 0;
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].dirty) count++;
    }
    return count;
}

int block_get_queue_pending_count(void) {
    return io_request_count;
}
