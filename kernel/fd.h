#ifndef FD_H
#define FD_H

#include <stdint.h>
#include <stddef.h>

/*
 * Day 10: I/O Subsystem
 * 
 * This implements file descriptors, standard I/O, and pipes for Oasis OS.
 * Each process has its own file descriptor table.
 */

/* Maximum file descriptors per process */
#define FD_MAX          16

/* Maximum number of system-wide pipes */
#define PIPE_MAX        8

/* Pipe buffer size */
#define PIPE_BUFFER_SIZE 512

/* Standard file descriptors */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* File descriptor types */
typedef enum {
    FD_TYPE_NONE = 0,       /* Unused slot */
    FD_TYPE_CONSOLE,        /* Console I/O (stdin/stdout/stderr) */
    FD_TYPE_PIPE_READ,      /* Read end of a pipe */
    FD_TYPE_PIPE_WRITE,     /* Write end of a pipe */
    FD_TYPE_FILE            /* Future: regular file */
} fd_type_t;

/* File descriptor flags */
#define FD_FLAG_READ    (1 << 0)
#define FD_FLAG_WRITE   (1 << 1)
#define FD_FLAG_APPEND  (1 << 2)
#define FD_FLAG_NONBLOCK (1 << 3)

/* Open flags (similar to POSIX) */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_CREAT         0x0040
#define O_TRUNC         0x0200
#define O_APPEND        0x0400

/* Seek whence values */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* Pipe structure - circular buffer */
typedef struct pipe {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint32_t read_pos;          /* Read position in buffer */
    uint32_t write_pos;         /* Write position in buffer */
    uint32_t count;             /* Bytes currently in buffer */
    uint32_t readers;           /* Number of read ends open */
    uint32_t writers;           /* Number of write ends open */
    uint8_t active;             /* Is pipe in use? */
} pipe_t;

/* File descriptor entry */
typedef struct fd_entry {
    fd_type_t type;             /* Type of this fd */
    uint32_t flags;             /* Read/write flags */
    uint32_t ref_count;         /* Reference count for sharing */
    uint32_t offset;            /* Current position (for files) */
    union {
        pipe_t *pipe;           /* For pipe types */
        uint32_t device_id;     /* For device types */
    } data;
} fd_entry_t;

/* Per-process file descriptor table */
typedef struct fd_table {
    fd_entry_t entries[FD_MAX];
} fd_table_t;

/* ====== Initialization ====== */

/* Initialize the I/O subsystem */
void fd_init(void);

/* Initialize file descriptor table for a new process */
void fd_table_init(fd_table_t *table);

/* Copy file descriptor table (for fork) */
void fd_table_copy(fd_table_t *dest, fd_table_t *src);

/* Close all file descriptors in a table (for process exit) */
void fd_table_close_all(fd_table_t *table);

/* ====== File Descriptor Operations ====== */

/* Open a file/device, returns fd or -1 on error */
int fd_open(fd_table_t *table, const char *path, int flags);

/* Close a file descriptor */
int fd_close(fd_table_t *table, int fd);

/* Read from a file descriptor */
int fd_read(fd_table_t *table, int fd, void *buf, uint32_t count);

/* Write to a file descriptor */
int fd_write(fd_table_t *table, int fd, const void *buf, uint32_t count);

/* Seek within a file descriptor */
int fd_seek(fd_table_t *table, int fd, int32_t offset, int whence);

/* Duplicate a file descriptor to lowest available fd */
int fd_dup(fd_table_t *table, int oldfd);

/* Duplicate a file descriptor to a specific fd */
int fd_dup2(fd_table_t *table, int oldfd, int newfd);

/* ====== Pipe Operations ====== */

/* Create a pipe, returns 0 on success, -1 on error
 * pipefd[0] = read end, pipefd[1] = write end */
int fd_pipe(fd_table_t *table, int pipefd[2]);

/* ====== Console Operations ====== */

/* Read a character from console (blocking) */
int console_read(void *buf, uint32_t count);

/* Write to console */
int console_write(const void *buf, uint32_t count);

/* ====== Utility Functions ====== */

/* Check if fd is valid */
int fd_is_valid(fd_table_t *table, int fd);

/* Get fd type */
fd_type_t fd_get_type(fd_table_t *table, int fd);

/* Debug: print fd table info */
void fd_print_table(fd_table_t *table);

/* Get current process's fd table */
fd_table_t *fd_get_current_table(void);

#endif /* FD_H */
