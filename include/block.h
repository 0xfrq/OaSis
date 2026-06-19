#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

// Ukuran block (sama kayak ukuran sektor ATA)
#define BLOCK_SIZE 512

// Jumlah maksimal block yang di-cache
#define BLOCK_CACHE_SIZE 32

// Ukuran antrian I/O
#define IO_QUEUE_SIZE 16

// Entry cache block
typedef struct {
    uint32_t block_num;     // Nomor block (LBA)
    uint8_t data[BLOCK_SIZE]; // Data block
    int dirty;              // 1 kalo udah diubah dan perlu ditulis
    int valid;              // 1 kalo entry cache-nya valid
    int ref_count;          // Jumlah referensi
} block_cache_entry_t;

// Tipe request I/O
typedef enum {
    IO_READ,
    IO_WRITE
} io_operation_t;

// Struktur request I/O
typedef struct io_request {
    io_operation_t operation;
    uint32_t block_num;
    uint8_t *buffer;
    int completed;
    int success;
    struct io_request *next;
} io_request_t;

// Prototype fungsi
void block_init(void);
int block_read(uint32_t block_num, uint8_t *buffer);
int block_write(uint32_t block_num, const uint8_t *buffer);
void block_flush(void); // Tulis semua block yang dirty ke disk

// Operasi antrian (buat I/O async)
void block_queue_init(void);
int block_queue_request(io_operation_t op, uint32_t block_num, uint8_t *buffer);
void block_process_queue(void);

// Fungsi info
int block_get_cache_valid_count(void);
int block_get_cache_dirty_count(void);
int block_get_queue_pending_count(void);

#endif
