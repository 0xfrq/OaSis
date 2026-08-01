# Virtual filesystem (VFS)

this page explains VFS layer in OaSis.

## Contents

- [apa itu VFS](#apa-itu-VFS)
- [architecture VFS](#architecture-VFS)
- [open file table](#open-file-table)
- [inode operations](#inode-operations)

---

## Apa itu VFS

**VFS (virtual filesystem)** is abstraction layer that create all filesystem terlihat same.

### Why butuh VFS?

bayangin kamu memiliki 2 filesystem:
- OAFS on a hard disk
- FAT32 on a USB drive

without VFS:
```c
// read from OAFS
OAFS_read(inode, offset, buf, size);

// read from FAT32
fat32_read(cluster, offset, buf, size);
```

with VFS:
```c
// read from any filesystem
int fd = VFS_open(path, O_RDONLY);
VFS_read(fd, buf, size);
VFS_close(fd);
```

### Manfaat

- **unified API**: one API for all filesystem
- **swap filesystem**: gampang ganti filesystem without ubah application
- **mount points**: can mount multiple filesystem

## Architecture VFS

```text
┌─────────────────────────────────────┐
│           aplikasi                  │
├─────────────────────────────────────┤
│         VFS layer                   │
│  VFS_open, VFS_read, VFS_write ... │
├─────────────────────────────────────┤
│       filesystem drivers            │
│  OAFS │ fat32 │ ext2 │ ...         │
├─────────────────────────────────────┤
│         block device layer          │
│  ata_read_sector, ata_write_sector  │
├─────────────────────────────────────┤
│           hardware                  │
└─────────────────────────────────────┘
```

## Open file table

VFS maintain table for track file that lagi kebuka.

### Structure

```c
# Define MAX_OPEN_FILES 64

typedef struct {
    uint32_t inode;     // inode number
    uint32_t offset;    // current read/write position
    uint32_t flags;     // open flags (read/write/append)
    uint32_t ref_count; // reference count
    uint8_t active;     // slot aktif apa gak
} open_file_t;

open_file_t open_files[MAX_OPEN_FILES];
```

### Operations

**alloc slot:**
```c
int alloc_open_file(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].active) {
            open_files[i].active = 1;
            open_files[i].ref_count = 1;
            return i;
        }
    }
    return -1;  // out of slots
}
```

**release slot:**
```c
void release_open_file(int fd) {
    open_files[fd].ref_count--;
    if (open_files[fd].ref_count == 0) {
        open_files[fd].active = 0;
    }
}
```

## Inode operations

VFS define standard operations that harus di-implement same filesystem.

### File operations

| operations | deskripsi |
|---------|-----------|
| `open(path, flags)` | buka file |
| `close(fd)` | tutup file |
| `read(fd, buf, size)` | read from file |
| `write(fd, buf, size)` | write to a file |
| `seek(fd, offset, whence)` | move the position |
| `stat(path, buf)` | get file information |

### Directory operations

| operations | deskripsi |
|---------|-----------|
| `mkdir(path)` | create directory |
| `rmdir(path)` | hapus directory |
| `readdir(path, entry)` | read isi directory |
| `chdir(path)` | change the current directory |
| `getcwd(buf, size)` | get the current directory |

### Management operations

| operations | deskripsi |
|---------|-----------|
| `create(path)` | create file new |
| `unlink(path)` | hapus file |
| `rename(old, new)` | rename file/directory |
| `truncate(path, size)` | potong file |

---

## Current working directory

VFS track current working directory (cwd).

```c
typedef struct {
    uint32_t inode;
    char path[256];
} cwd_t;

cwd_t current_dir;
```

### Chdir

```c
int VFS_chdir(const char *path) {
    uint32_t inode;
    if (resolve_path(path, &inode) != 0) {
        return -1;
    }
    
    current_dir.inode = inode;
    strncpy(current_dir.path, path, 256);
    return 0;
}
```

### Getcwd

```c
void VFS_getcwd(char *buf, uint32_t size) {
    strncpy(buf, current_dir.path, size);
}
```

---

**back to:** [filesystem →](readme.md)
