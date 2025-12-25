#ifndef NULL
#define NULL ((void*)0)
#endif
#include "block.h"
#include "ata.h"
#include <stdint.h>

// Simple memcpy implementation
static void memcpy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// Block cache
static block_cache_entry_t block_cache[BLOCK_CACHE_SIZE];

// I/O request queue (static allocation)
static io_request_t io_requests[IO_QUEUE_SIZE];
static int io_request_count = 0;

// Initialize block device layer
void block_init(void) {
    ata_init();

    // Initialize cache
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        block_cache[i].valid = 0;
        block_cache[i].dirty = 0;
        block_cache[i].ref_count = 0;
    }

    // Initialize I/O queue
    block_queue_init();
}

// Find a cache entry for the given block, or find an empty slot
static block_cache_entry_t *find_cache_entry(uint32_t block_num) {
    // First, look for existing entry
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].block_num == block_num) {
            return &block_cache[i];
        }
    }

    // Not found, look for empty slot
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (!block_cache[i].valid) {
            return &block_cache[i];
        }
    }

    // No empty slot, use LRU (simple: first valid entry)
    // In a real implementation, we'd track access times
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].ref_count == 0) {
            // Write back if dirty
            if (block_cache[i].dirty) {
                ata_write_sector(block_cache[i].block_num, block_cache[i].data);
                block_cache[i].dirty = 0;
            }
            block_cache[i].valid = 0;
            return &block_cache[i];
        }
    }

    // All entries are in use, return NULL (shouldn't happen with proper ref counting)
    return NULL;
}

// Read a block, using cache
int block_read(uint32_t block_num, uint8_t *buffer) {
    block_cache_entry_t *entry = find_cache_entry(block_num);

    if (!entry) {
        // Cache full, read directly
        return ata_read_sector(block_num, buffer);
    }

    if (!entry->valid || entry->block_num != block_num) {
        // Load from disk
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

// Write a block, using cache
int block_write(uint32_t block_num, const uint8_t *buffer) {
    block_cache_entry_t *entry = find_cache_entry(block_num);

    if (!entry) {
        // Cache full, write directly
        return ata_write_sector(block_num, buffer);
    }

    // Copy data to cache
    memcpy(entry->data, buffer, BLOCK_SIZE);
    entry->block_num = block_num;
    entry->valid = 1;
    entry->dirty = 1;
    entry->ref_count++;

    // For write-through cache, also write to disk immediately
    int result = ata_write_sector(block_num, buffer);
    if (result == 0) {
        entry->dirty = 0; // Clear dirty flag
    }
    entry->ref_count--;

    return result;
}

// Flush all dirty blocks to disk
void block_flush(void) {
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (block_cache[i].valid && block_cache[i].dirty) {
            ata_write_sector(block_cache[i].block_num, block_cache[i].data);
            block_cache[i].dirty = 0;
        }
    }
}

// Initialize I/O request queue
void block_queue_init(void) {
    io_request_count = 0;
}

// Add a request to the I/O queue
int block_queue_request(io_operation_t op, uint32_t block_num, uint8_t *buffer) {
    if (io_request_count >= IO_QUEUE_SIZE) return -1; // Queue full

    io_request_t *req = &io_requests[io_request_count++];
    req->operation = op;
    req->block_num = block_num;
    req->buffer = buffer;
    req->completed = 0;
    req->success = 0;

    return 0;
}

// Process pending I/O requests
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

// Get cache statistics
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