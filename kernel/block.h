#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

// Block size (matches ATA sector size)
#define BLOCK_SIZE 512

// Maximum number of cached blocks
#define BLOCK_CACHE_SIZE 32

// I/O queue size
#define IO_QUEUE_SIZE 16

// Block cache entry
typedef struct {
    uint32_t block_num;     // Block number (LBA)
    uint8_t data[BLOCK_SIZE]; // Block data
    int dirty;              // 1 if modified and needs writing
    int valid;              // 1 if cache entry is valid
    int ref_count;          // Reference count
} block_cache_entry_t;

// I/O request types
typedef enum {
    IO_READ,
    IO_WRITE
} io_operation_t;

// I/O request structure
typedef struct io_request {
    io_operation_t operation;
    uint32_t block_num;
    uint8_t *buffer;
    int completed;
    int success;
    struct io_request *next;
} io_request_t;

// Function prototypes
void block_init(void);
int block_read(uint32_t block_num, uint8_t *buffer);
int block_write(uint32_t block_num, const uint8_t *buffer);
void block_flush(void); // Write all dirty blocks to disk

// Queue operations (for async I/O)
void block_queue_init(void);
int block_queue_request(io_operation_t op, uint32_t block_num, uint8_t *buffer);
void block_process_queue(void);

// Information functions
int block_get_cache_valid_count(void);
int block_get_cache_dirty_count(void);
int block_get_queue_pending_count(void);

#endif