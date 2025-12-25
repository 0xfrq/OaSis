/*
 * Day 10: I/O Subsystem Implementation
 * 
 * Implements file descriptors, standard I/O, and pipes for Oasis OS.
 */

#include "fd.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "task.h"

/* Global pipe pool */
static pipe_t pipes[PIPE_MAX];

/* Default console fd table (used before tasks are running) */
static fd_table_t kernel_fd_table;

/* ====== Helper Functions ====== */

static int find_free_fd(fd_table_t *table) {
    for (int i = 0; i < FD_MAX; i++) {
        if (table->entries[i].type == FD_TYPE_NONE) {
            return i;
        }
    }
    return -1;  /* No free fd */
}

static int find_free_fd_from(fd_table_t *table, int start) {
    for (int i = start; i < FD_MAX; i++) {
        if (table->entries[i].type == FD_TYPE_NONE) {
            return i;
        }
    }
    return -1;
}

static pipe_t *alloc_pipe(void) {
    for (int i = 0; i < PIPE_MAX; i++) {
        if (!pipes[i].active) {
            pipes[i].active = 1;
            pipes[i].read_pos = 0;
            pipes[i].write_pos = 0;
            pipes[i].count = 0;
            pipes[i].readers = 0;
            pipes[i].writers = 0;
            return &pipes[i];
        }
    }
    return NULL;
}

static void free_pipe(pipe_t *pipe) {
    if (pipe) {
        pipe->active = 0;
    }
}

/* ====== Initialization ====== */

void fd_init(void) {
    vga_print("[*] Initializing I/O subsystem...\n");
    
    /* Initialize pipe pool */
    for (int i = 0; i < PIPE_MAX; i++) {
        pipes[i].active = 0;
    }
    
    /* Initialize kernel fd table with standard I/O */
    fd_table_init(&kernel_fd_table);
    
    vga_print("[+] I/O subsystem initialized\n");
    vga_print("    - File descriptors: ");
    char buf[16];
    itoa(FD_MAX, buf, 10);
    vga_print(buf);
    vga_print(" per process\n");
    vga_print("    - Pipes available: ");
    itoa(PIPE_MAX, buf, 10);
    vga_print(buf);
    vga_print("\n");
    vga_print("    - stdin=0, stdout=1, stderr=2\n");
}

void fd_table_init(fd_table_t *table) {
    if (!table) return;
    
    /* Clear all entries */
    for (int i = 0; i < FD_MAX; i++) {
        table->entries[i].type = FD_TYPE_NONE;
        table->entries[i].flags = 0;
        table->entries[i].ref_count = 0;
        table->entries[i].offset = 0;
        table->entries[i].data.pipe = NULL;
    }
    
    /* Setup standard file descriptors */
    
    /* stdin (fd 0) - console read */
    table->entries[STDIN_FILENO].type = FD_TYPE_CONSOLE;
    table->entries[STDIN_FILENO].flags = FD_FLAG_READ;
    table->entries[STDIN_FILENO].ref_count = 1;
    
    /* stdout (fd 1) - console write */
    table->entries[STDOUT_FILENO].type = FD_TYPE_CONSOLE;
    table->entries[STDOUT_FILENO].flags = FD_FLAG_WRITE;
    table->entries[STDOUT_FILENO].ref_count = 1;
    
    /* stderr (fd 2) - console write */
    table->entries[STDERR_FILENO].type = FD_TYPE_CONSOLE;
    table->entries[STDERR_FILENO].flags = FD_FLAG_WRITE;
    table->entries[STDERR_FILENO].ref_count = 1;
}

void fd_table_copy(fd_table_t *dest, fd_table_t *src) {
    if (!dest || !src) return;
    
    for (int i = 0; i < FD_MAX; i++) {
        dest->entries[i] = src->entries[i];
        
        /* Increase reference counts */
        if (src->entries[i].type != FD_TYPE_NONE) {
            dest->entries[i].ref_count++;
            
            /* For pipes, increase reader/writer count */
            if (src->entries[i].type == FD_TYPE_PIPE_READ && src->entries[i].data.pipe) {
                src->entries[i].data.pipe->readers++;
            } else if (src->entries[i].type == FD_TYPE_PIPE_WRITE && src->entries[i].data.pipe) {
                src->entries[i].data.pipe->writers++;
            }
        }
    }
}

void fd_table_close_all(fd_table_t *table) {
    if (!table) return;
    
    for (int i = 0; i < FD_MAX; i++) {
        if (table->entries[i].type != FD_TYPE_NONE) {
            fd_close(table, i);
        }
    }
}

/* ====== Console I/O ====== */

int console_read(void *buf, uint32_t count) {
    if (!buf || count == 0) return 0;
    
    char *cbuf = (char *)buf;
    uint32_t read_count = 0;
    
    /* Read characters until count reached or newline */
    while (read_count < count) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            cbuf[read_count++] = c;
            break;
        }
        
        if (c == '\b') {
            if (read_count > 0) {
                read_count--;
                vga_putc('\b');  /* Echo backspace */
            }
            continue;
        }
        
        cbuf[read_count++] = c;
        vga_putc(c);  /* Echo character */
    }
    
    return read_count;
}

int console_write(const void *buf, uint32_t count) {
    if (!buf || count == 0) return 0;
    
    const char *cbuf = (const char *)buf;
    
    for (uint32_t i = 0; i < count; i++) {
        vga_putc(cbuf[i]);
    }
    
    return count;
}

/* ====== File Descriptor Operations ====== */

int fd_is_valid(fd_table_t *table, int fd) {
    if (!table) return 0;
    if (fd < 0 || fd >= FD_MAX) return 0;
    if (table->entries[fd].type == FD_TYPE_NONE) return 0;
    return 1;
}

fd_type_t fd_get_type(fd_table_t *table, int fd) {
    if (!fd_is_valid(table, fd)) return FD_TYPE_NONE;
    return table->entries[fd].type;
}

int fd_open(fd_table_t *table, const char *path, int flags) {
    if (!table || !path) return -1;
    
    int fd = find_free_fd(table);
    if (fd < 0) return -1;
    
    fd_entry_t *entry = &table->entries[fd];
    
    /* Special device paths */
    if (strcmp(path, "/dev/console") == 0 || 
        strcmp(path, "/dev/tty") == 0) {
        entry->type = FD_TYPE_CONSOLE;
        entry->flags = FD_FLAG_READ | FD_FLAG_WRITE;
        entry->ref_count = 1;
        return fd;
    }
    
    if (strcmp(path, "/dev/stdin") == 0) {
        entry->type = FD_TYPE_CONSOLE;
        entry->flags = FD_FLAG_READ;
        entry->ref_count = 1;
        return fd;
    }
    
    if (strcmp(path, "/dev/stdout") == 0 ||
        strcmp(path, "/dev/stderr") == 0) {
        entry->type = FD_TYPE_CONSOLE;
        entry->flags = FD_FLAG_WRITE;
        entry->ref_count = 1;
        return fd;
    }
    
    /* For now, other paths are not supported (no filesystem yet) */
    /* This will be expanded in Day 11-12 */
    (void)flags;
    
    return -1;  /* File not found / not supported */
}

int fd_close(fd_table_t *table, int fd) {
    if (!fd_is_valid(table, fd)) return -1;
    
    fd_entry_t *entry = &table->entries[fd];
    
    /* Handle pipe cleanup */
    if (entry->type == FD_TYPE_PIPE_READ && entry->data.pipe) {
        entry->data.pipe->readers--;
        if (entry->data.pipe->readers == 0 && entry->data.pipe->writers == 0) {
            free_pipe(entry->data.pipe);
        }
    } else if (entry->type == FD_TYPE_PIPE_WRITE && entry->data.pipe) {
        entry->data.pipe->writers--;
        if (entry->data.pipe->readers == 0 && entry->data.pipe->writers == 0) {
            free_pipe(entry->data.pipe);
        }
    }
    
    /* Clear the entry */
    entry->type = FD_TYPE_NONE;
    entry->flags = 0;
    entry->ref_count = 0;
    entry->offset = 0;
    entry->data.pipe = NULL;
    
    return 0;
}

int fd_read(fd_table_t *table, int fd, void *buf, uint32_t count) {
    if (!fd_is_valid(table, fd)) return -1;
    if (!buf || count == 0) return 0;
    
    fd_entry_t *entry = &table->entries[fd];
    
    /* Check read permission */
    if (!(entry->flags & FD_FLAG_READ)) {
        return -1;  /* Not readable */
    }
    
    switch (entry->type) {
        case FD_TYPE_CONSOLE:
            return console_read(buf, count);
            
        case FD_TYPE_PIPE_READ: {
            pipe_t *pipe = entry->data.pipe;
            if (!pipe) return -1;
            
            /* Wait for data or EOF (writers closed) */
            while (pipe->count == 0) {
                if (pipe->writers == 0) {
                    return 0;  /* EOF - no more writers */
                }
                /* In a real OS, we'd block here. For now, busy wait */
                /* Could call task_yield() to allow other tasks to run */
            }
            
            /* Read from pipe buffer */
            uint32_t to_read = (count < pipe->count) ? count : pipe->count;
            uint8_t *cbuf = (uint8_t *)buf;
            
            for (uint32_t i = 0; i < to_read; i++) {
                cbuf[i] = pipe->buffer[pipe->read_pos];
                pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
                pipe->count--;
            }
            
            return to_read;
        }
        
        case FD_TYPE_FILE:
            /* Will be implemented in Day 11-12 */
            return -1;
            
        default:
            return -1;
    }
}

int fd_write(fd_table_t *table, int fd, const void *buf, uint32_t count) {
    if (!fd_is_valid(table, fd)) return -1;
    if (!buf || count == 0) return 0;
    
    fd_entry_t *entry = &table->entries[fd];
    
    /* Check write permission */
    if (!(entry->flags & FD_FLAG_WRITE)) {
        return -1;  /* Not writable */
    }
    
    switch (entry->type) {
        case FD_TYPE_CONSOLE:
            return console_write(buf, count);
            
        case FD_TYPE_PIPE_WRITE: {
            pipe_t *pipe = entry->data.pipe;
            if (!pipe) return -1;
            
            /* Check if anyone is reading */
            if (pipe->readers == 0) {
                return -1;  /* Broken pipe - no readers */
            }
            
            /* Write to pipe buffer */
            const uint8_t *cbuf = (const uint8_t *)buf;
            uint32_t written = 0;
            
            while (written < count) {
                /* Wait for space in buffer */
                while (pipe->count >= PIPE_BUFFER_SIZE) {
                    if (pipe->readers == 0) {
                        return written > 0 ? written : -1;
                    }
                    /* In a real OS, we'd block here */
                }
                
                pipe->buffer[pipe->write_pos] = cbuf[written];
                pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_SIZE;
                pipe->count++;
                written++;
            }
            
            return written;
        }
        
        case FD_TYPE_FILE:
            /* Will be implemented in Day 11-12 */
            return -1;
            
        default:
            return -1;
    }
}

int fd_seek(fd_table_t *table, int fd, int32_t offset, int whence) {
    if (!fd_is_valid(table, fd)) return -1;
    
    fd_entry_t *entry = &table->entries[fd];
    
    /* Only files support seeking */
    if (entry->type != FD_TYPE_FILE) {
        return -1;
    }
    
    /* Will be fully implemented in Day 11-12 */
    (void)offset;
    (void)whence;
    
    return -1;
}

int fd_dup(fd_table_t *table, int oldfd) {
    if (!fd_is_valid(table, oldfd)) return -1;
    
    int newfd = find_free_fd(table);
    if (newfd < 0) return -1;
    
    return fd_dup2(table, oldfd, newfd);
}

int fd_dup2(fd_table_t *table, int oldfd, int newfd) {
    if (!fd_is_valid(table, oldfd)) return -1;
    if (newfd < 0 || newfd >= FD_MAX) return -1;
    
    /* If newfd is already open, close it first */
    if (table->entries[newfd].type != FD_TYPE_NONE) {
        fd_close(table, newfd);
    }
    
    /* Copy the fd entry */
    table->entries[newfd] = table->entries[oldfd];
    table->entries[newfd].ref_count++;
    
    /* Update pipe reference counts */
    if (table->entries[oldfd].type == FD_TYPE_PIPE_READ && table->entries[oldfd].data.pipe) {
        table->entries[oldfd].data.pipe->readers++;
    } else if (table->entries[oldfd].type == FD_TYPE_PIPE_WRITE && table->entries[oldfd].data.pipe) {
        table->entries[oldfd].data.pipe->writers++;
    }
    
    return newfd;
}

/* ====== Pipe Operations ====== */

int fd_pipe(fd_table_t *table, int pipefd[2]) {
    if (!table || !pipefd) return -1;
    
    /* Allocate a pipe */
    pipe_t *pipe = alloc_pipe();
    if (!pipe) return -1;
    
    /* Find two free file descriptors */
    int read_fd = find_free_fd(table);
    if (read_fd < 0) {
        free_pipe(pipe);
        return -1;
    }
    
    /* Mark it temporarily so find_free_fd_from doesn't return same fd */
    table->entries[read_fd].type = FD_TYPE_PIPE_READ;
    
    int write_fd = find_free_fd_from(table, read_fd + 1);
    if (write_fd < 0) {
        table->entries[read_fd].type = FD_TYPE_NONE;
        free_pipe(pipe);
        return -1;
    }
    
    /* Setup read end */
    table->entries[read_fd].type = FD_TYPE_PIPE_READ;
    table->entries[read_fd].flags = FD_FLAG_READ;
    table->entries[read_fd].ref_count = 1;
    table->entries[read_fd].data.pipe = pipe;
    pipe->readers = 1;
    
    /* Setup write end */
    table->entries[write_fd].type = FD_TYPE_PIPE_WRITE;
    table->entries[write_fd].flags = FD_FLAG_WRITE;
    table->entries[write_fd].ref_count = 1;
    table->entries[write_fd].data.pipe = pipe;
    pipe->writers = 1;
    
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    return 0;
}

/* ====== Utility Functions ====== */

void fd_print_table(fd_table_t *table) {
    if (!table) {
        vga_print("FD Table: NULL\n");
        return;
    }
    
    vga_print("File Descriptor Table:\n");
    char buf[16];
    
    for (int i = 0; i < FD_MAX; i++) {
        if (table->entries[i].type != FD_TYPE_NONE) {
            vga_print("  fd ");
            itoa(i, buf, 10);
            vga_print(buf);
            vga_print(": ");
            
            switch (table->entries[i].type) {
                case FD_TYPE_CONSOLE:
                    vga_print("CONSOLE");
                    break;
                case FD_TYPE_PIPE_READ:
                    vga_print("PIPE_R");
                    break;
                case FD_TYPE_PIPE_WRITE:
                    vga_print("PIPE_W");
                    break;
                case FD_TYPE_FILE:
                    vga_print("FILE");
                    break;
                default:
                    vga_print("UNKNOWN");
                    break;
            }
            
            vga_print(" flags=");
            if (table->entries[i].flags & FD_FLAG_READ) vga_print("R");
            if (table->entries[i].flags & FD_FLAG_WRITE) vga_print("W");
            
            vga_print(" ref=");
            itoa(table->entries[i].ref_count, buf, 10);
            vga_print(buf);
            vga_print("\n");
        }
    }
}

fd_table_t *fd_get_current_table(void) {
    task_t *current = task_get_current();
    if (current && current->fd_table) {
        return current->fd_table;
    }
    return &kernel_fd_table;
}

/* ====== Legacy Write Compatibility ====== */

/* This replaces the old direct sys_write that went to VGA */
int fd_sys_write(int fd, const void *buf, uint32_t count) {
    fd_table_t *table = fd_get_current_table();
    return fd_write(table, fd, buf, count);
}

/* Legacy write to stdout for compatibility */
int fd_write_stdout(const void *buf, uint32_t count) {
    return fd_sys_write(STDOUT_FILENO, buf, count);
}
