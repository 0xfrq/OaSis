#ifndef VFS_H
#define VFS_H

#include <stdint.h>

/*
 * Oasis File System (OAFS) - Filesystem sederhana buat OS pembelajaran
 *
 * Layout Disk:
 * [Superblock] [Tabel Inode] [Block Data]
 *
 * - Superblock: Block 0, isinya metadata filesystem
 * - Tabel Inode: Block 1-64, bisa sampe 1024 inode
 * - Block Data: Block 64 ke atas, isinya file/direktori
 */

#define VFS_MAGIC 0x0AF6

#define MAX_INODES 1024
#define MAX_OPEN_FILES 32
#define MAX_PATH_LENGTH 256
#define MAX_FILENAME_LENGTH 32
/* Satu direktori sekarang muat di satu block 512-byte: 512 / (4 + 32) = 14 entries */
#define MAX_DIR_ENTRIES 168

#define INODE_TYPE_FREE 0
#define INODE_TYPE_FILE 1
#define INODE_TYPE_DIR 2

#define VFS_O_READ 0x01
#define VFS_O_WRITE 0x02
#define VFS_O_CREATE 0x04
#define VFS_O_APPEND 0x08
#define VFS_O_TRUNC 0x10

/* Superblock - disimpen di block 0 */
typedef struct {
    uint32_t magic;             // Harus VFS_MAGIC
    uint32_t total_blocks;      // Total block di filesystem
    uint32_t inode_table_start; // Block tempat tabel inode mulai
    uint32_t data_start;        // Block tempat block data mulai
    uint32_t total_inodes;      // Total inode yang ada
    uint32_t free_inodes;       // Inode yang belum kepake
    uint32_t free_blocks;       // Block data yang belum kepake
} __attribute__((packed)) superblock_t;

/* Inode - metadata file/direktori */
typedef struct {
    uint32_t type;              // INODE_TYPE_*
    uint32_t size;              // Ukuran file dalam byte
    uint32_t start_block;       // Block data pertama (0 kalo kosong)
    uint32_t parent_inode;      // Nomor inode direktori parent
    uint32_t ctime;             // Waktu dibikin (ticks)
    uint32_t mtime;             // Waktu diubah (ticks)
    uint32_t direct[12];        // Pointer block langsung (file sampe 48KB)
    uint32_t indirect;          // Pointer block gak langsung (file sampe 12MB)
    char name[MAX_FILENAME_LENGTH]; // Nama file
} __attribute__((packed)) inode_t;

/* Entry direktori */
typedef struct {
    uint32_t inode_number;      // Indeks inode di tabel inode
    char name[MAX_FILENAME_LENGTH];
} __attribute__((packed)) dir_entry_t;

/* File descriptor yang lagi kebuka */
typedef struct {
    uint32_t inode_number;
    uint32_t offset;            // Posisi baca/tulis sekarang
    uint32_t flags;             // O_READ, O_WRITE, dll.
    int ref_count;              // Jumlah referensi
} open_file_t;

/* Direktori kerja sekarang */
typedef struct {
    uint32_t inode_number;
    char path[MAX_PATH_LENGTH];
} cwd_t;

/* State filesystem */
typedef struct {
    int initialized;
    superblock_t superblock;
    inode_t inodes[MAX_INODES];
    open_file_t open_files[MAX_OPEN_FILES];
    cwd_t cwd;
    uint32_t timer_ticks;       // Buat timestamp
} vfs_state_t;

/* Prototype fungsi */
int vfs_init(void);
int vfs_create(const char *path);
int vfs_open(const char *path, uint32_t flags);
int vfs_close(int fd);
int vfs_read(int fd, char *buf, uint32_t count);
int vfs_write(int fd, const char *buf, uint32_t count);
int vfs_seek(int fd, uint32_t offset);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);
int vfs_list(const char *path, char *buf, uint32_t max_len);
int vfs_chdir(const char *path);
int vfs_getcwd(char *buf, uint32_t max_len);
uint32_t vfs_get_ticks(void);
void vfs_set_ticks(uint32_t ticks);

/* Helper internal */
int vfs_resolve_path(const char *path, uint32_t *inode_out);
int vfs_alloc_inode(void);
void vfs_free_inode(uint32_t inode_num);
int vfs_alloc_block(void);
void vfs_free_block(uint32_t block_num);

#endif
