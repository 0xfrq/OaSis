#ifndef FD_H
#define FD_H

#include <stdint.h>
#include <stddef.h>

/*
 * Day 10: Subsystem I/O
 *
 * Ini bikin file descriptor, I/O standar, sama pipe buat Oasis OS.
 * Tiap proses punya tabel file descriptor sendiri.
 */

/* Jumlah maksimal file descriptor per proses */
#define FD_MAX          16

/* Jumlah maksimal pipe di seluruh sistem */
#define PIPE_MAX        8

/* Ukuran buffer pipe */
#define PIPE_BUFFER_SIZE 512

/* File descriptor standar */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* Tipe file descriptor */
typedef enum {
    FD_TYPE_NONE = 0,       /* Slot gak kepake */
    FD_TYPE_CONSOLE,        /* I/O console (stdin/stdout/stderr) */
    FD_TYPE_PIPE_READ,      /* Ujung baca pipe */
    FD_TYPE_PIPE_WRITE,     /* Ujung tulis pipe */
    FD_TYPE_FILE            /* Buat nanti: file biasa */
} fd_type_t;

/* Flag file descriptor */
#define FD_FLAG_READ    (1 << 0)
#define FD_FLAG_WRITE   (1 << 1)
#define FD_FLAG_APPEND  (1 << 2)
#define FD_FLAG_NONBLOCK (1 << 3)

/* Flag open (mirip-mirip POSIX) */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_CREAT         0x0040
#define O_TRUNC         0x0200
#define O_APPEND        0x0400

/* Nilai whence buat seek */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* Struktur pipe - buffer melingkar */
typedef struct pipe {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint32_t read_pos;          /* Posisi baca di buffer */
    uint32_t write_pos;         /* Posisi tulis di buffer */
    uint32_t count;             /* Byte yang lagi ada di buffer */
    uint32_t readers;           /* Jumlah ujung baca yang kebuka */
    uint32_t writers;           /* Jumlah ujung tulis yang kebuka */
    uint8_t active;             /* Pipe lagi dipake gak? */
} pipe_t;

/* Entry file descriptor */
typedef struct fd_entry {
    fd_type_t type;             /* Tipe fd ini */
    uint32_t flags;             /* Flag read/write */
    uint32_t ref_count;         /* Hitungan referensi buat sharing */
    uint32_t offset;            /* Posisi sekarang (buat file) */
    union {
        pipe_t *pipe;           /* Buat tipe pipe */
        uint32_t device_id;     /* Buat tipe device */
        int vfs_fd;             /* FD file VFS yang kebuka (buat tipe file) */
    } data;
} fd_entry_t;

/* Tabel file descriptor per proses */
typedef struct fd_table {
    fd_entry_t entries[FD_MAX];
} fd_table_t;

/* ====== Inisialisasi ====== */

/* Inisialisasi subsystem I/O */
void fd_init(void);

/* Inisialisasi tabel fd buat proses baru */
void fd_table_init(fd_table_t *table);

/* Copy tabel fd (buat fork) */
void fd_table_copy(fd_table_t *dest, fd_table_t *src);

/* Tutup semua fd di tabel (buat proses keluar) */
void fd_table_close_all(fd_table_t *table);

/* ====== Operasi File Descriptor ====== */

/* Buka file/device, return fd atau -1 kalo error */
int fd_open(fd_table_t *table, const char *path, int flags);

/* Tutup file descriptor */
int fd_close(fd_table_t *table, int fd);

/* Baca dari file descriptor */
int fd_read(fd_table_t *table, int fd, void *buf, uint32_t count);

/* Tulis ke file descriptor */
int fd_write(fd_table_t *table, int fd, const void *buf, uint32_t count);

/* Seek di file descriptor */
int fd_seek(fd_table_t *table, int fd, int32_t offset, int whence);

/* Duplikat fd ke fd terendah yang kosong */
int fd_dup(fd_table_t *table, int oldfd);

/* Duplikat fd ke fd tertentu */
int fd_dup2(fd_table_t *table, int oldfd, int newfd);

/* ====== Operasi Pipe ====== */

/* Bikin pipe, return 0 kalo sukses, -1 kalo error
 * pipefd[0] = ujung baca, pipefd[1] = ujung tulis */
int fd_pipe(fd_table_t *table, int pipefd[2]);

/* ====== Operasi Console ====== */

/* Baca karakter dari console (blocking) */
int console_read(void *buf, uint32_t count);

/* Tulis ke console */
int console_write(const void *buf, uint32_t count);

/* ====== Fungsi Utilitas ====== */

/* Cek fd valid apa gak */
int fd_is_valid(fd_table_t *table, int fd);

/* Dapetin tipe fd */
fd_type_t fd_get_type(fd_table_t *table, int fd);

/* Debug: print info tabel fd */
void fd_print_table(fd_table_t *table);

/* Dapetin tabel fd proses sekarang */
fd_table_t *fd_get_current_table(void);

#endif /* FD_H */
