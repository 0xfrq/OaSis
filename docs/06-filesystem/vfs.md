# virtual filesystem (vfs)

dokumentasi ini ngebahas vfs layer di OaSis.

## daftar isi

- [apa itu vfs](#apa-itu-vfs)
- [arsitektur vfs](#arsitektur-vfs)
- [open file table](#open-file-table)
- [inode operations](#inode-operations)

---

## apa itu vfs

**vfs (virtual filesystem)** adalah abstraction layer yang bikin semua filesystem keliatan sama.

### kenapa butuh vfs?

bayangin kamu punya 2 filesystem:
- oafs di hard disk
- fat32 di usb drive

tanpa vfs:
```c
// baca dari oafs
oafs_read(inode, offset, buf, size);

// baca dari fat32
fat32_read(cluster, offset, buf, size);
```

dengan vfs:
```c
// baca dari mana aja
int fd = vfs_open(path, O_RDONLY);
vfs_read(fd, buf, size);
vfs_close(fd);
```

### manfaat

- **unified API**: satu API buat semua filesystem
- **swap filesystem**: gampang ganti filesystem tanpa ubah aplikasi
- **mount points**: bisa mount multiple filesystem

## arsitektur vfs

```
┌─────────────────────────────────────┐
│           aplikasi                  │
├─────────────────────────────────────┤
│         vfs layer                   │
│  vfs_open, vfs_read, vfs_write ... │
├─────────────────────────────────────┤
│       filesystem drivers            │
│  oafs │ fat32 │ ext2 │ ...         │
├─────────────────────────────────────┤
│         block device layer          │
│  ata_read_sector, ata_write_sector  │
├─────────────────────────────────────┤
│           hardware                  │
└─────────────────────────────────────┘
```

## open file table

vfs maintain table buat track file yang lagi kebuka.

### struktur

```c
#define MAX_OPEN_FILES 64

typedef struct {
    uint32_t inode;     // inode number
    uint32_t offset;    // current read/write position
    uint32_t flags;     // open flags (read/write/append)
    uint32_t ref_count; // reference count
    uint8_t active;     // slot aktif apa gak
} open_file_t;

open_file_t open_files[MAX_OPEN_FILES];
```

### operasi

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

## inode operations

vfs define standard operations yang harus di-implement sama filesystem.

### file operations

| operasi | deskripsi |
|---------|-----------|
| `open(path, flags)` | buka file |
| `close(fd)` | tutup file |
| `read(fd, buf, size)` | baca dari file |
| `write(fd, buf, size)` | tulis ke file |
| `seek(fd, offset, whence)` | pindah posisi |
| `stat(path, buf)` | dapetin file info |

### directory operations

| operasi | deskripsi |
|---------|-----------|
| `mkdir(path)` | bikin directory |
| `rmdir(path)` | hapus directory |
| `readdir(path, entry)` | baca isi directory |
| `chdir(path)` | pindah current directory |
| `getcwd(buf, size)` | dapetin current directory |

### management operations

| operasi | deskripsi |
|---------|-----------|
| `create(path)` | bikin file baru |
| `unlink(path)` | hapus file |
| `rename(old, new)` | rename file/directory |
| `truncate(path, size)` | potong file |

---

## current working directory

vfs track current working directory (cwd).

```c
typedef struct {
    uint32_t inode;
    char path[256];
} cwd_t;

cwd_t current_dir;
```

### chdir

```c
int vfs_chdir(const char *path) {
    uint32_t inode;
    if (resolve_path(path, &inode) != 0) {
        return -1;
    }
    
    current_dir.inode = inode;
    strncpy(current_dir.path, path, 256);
    return 0;
}
```

### getcwd

```c
void vfs_getcwd(char *buf, uint32_t size) {
    strncpy(buf, current_dir.path, size);
}
```

---

**kembali ke:** [filesystem →](readme.md)
