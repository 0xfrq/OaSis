# filesystem

dokumentasi ini ngebahas gimana OaSis ngatur file dan storage.

## daftar isi

- [overview](#overview)
- [oafs (oasis filesystem)](#oafs-oasis-filesystem)
- [vfs (virtual filesystem)](#vfs-virtual-filesystem)
- [file operations](#file-operations)
- [directory operations](#directory-operations)

---

## overview

**filesystem** di OaSis ngatur gimana data disimpen dan diorganisir di disk.

### kemampuan

- simpan file dan directory
- read/write file
- create/delete file dan directory
- hierarchical directory structure
- file permissions (basic)

### limitation

- single filesystem (belum support multiple mount points)
- no journaling (belum crash-safe)
- no compression/encryption
- max file size: 4 GB

## oafs (oasis filesystem)

**oafs** adalah custom filesystem yang dibuat khusus buat OaSis.

### karakteristik

- **block size**: 512 bytes (1 sector)
- **max file size**: 4 GB
- **max filename**: 255 characters
- **inode-based**: mirip ext2/ext3
- **simple design**: gampang dipelajari

### struktur disk

```
┌─────────────────────────────────────┐
│  boot sector (1 block)              │  block 0
├─────────────────────────────────────┤
│  superblock (1 block)               │  block 1
├─────────────────────────────────────┤
│  block bitmap (n blocks)            │  block 2 - ...
├─────────────────────────────────────┤
│  inode bitmap (n blocks)            │  ...
├─────────────────────────────────────┤
│  inode table (n blocks)             │  ...
├─────────────────────────────────────┤
│  data blocks (n blocks)             │  ... - end
└─────────────────────────────────────┘
```

detail ada di [structure.md](structure.md)

## vfs (virtual filesystem)

**vfs** adalah abstraction layer yang bikin semua filesystem keliatan sama dari sisi aplikasi.

### kenapa butuh vfs?

- aplikasi gak perlu tau detail filesystem
- gampang ganti filesystem (swap oafs dengan fat32, dll)
- unified API buat semua operasi file

### vfs api

```c
// file operations
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t size);
int vfs_write(int fd, const void *buf, size_t size);

// directory operations
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_readdir(const char *path, struct dirent *entry);

// file management
int vfs_create(const char *path);
int vfs_delete(const char *path);
int vfs_stat(const char *path, struct stat *st);
```

detail ada di [vfs.md](vfs.md)

## file operations

operasi dasar yang bisa dilakuin sama file:

### open file

```c
int fd = vfs_open("/home/user/file.txt", O_RDONLY);
if (fd < 0) {
    // error
}
```

flags:
- `O_RDONLY`: read only
- `O_WRONLY`: write only
- `O_RDWR`: read and write
- `O_CREAT`: create if not exists
- `O_TRUNC`: truncate to 0 length
- `O_APPEND`: append mode

### read file

```c
char buffer[1024];
int bytes_read = vfs_read(fd, buffer, sizeof(buffer));
if (bytes_read > 0) {
    // process data
}
```

### write file

```c
const char *data = "hello world\n";
int bytes_written = vfs_write(fd, data, strlen(data));
```

### close file

```c
vfs_close(fd);
```

detail ada di [operations.md](operations.md)

## directory operations

operasi dasar yang bisa dilakuin sama directory:

### create directory

```c
int ret = vfs_mkdir("/home/user/newdir");
if (ret == 0) {
    // success
}
```

### delete directory

```c
int ret = vfs_rmdir("/home/user/emptydir");
if (ret == 0) {
    // success
}
```

**catatan:** directory harus kosong sebelum dihapus

### list directory

```c
struct dirent entry;
while (vfs_readdir("/home/user", &entry) == 0) {
    printf("%s\n", entry.name);
}
```

### change directory

```c
int ret = vfs_chdir("/home/user");
if (ret == 0) {
    // success
}
```

---

## contoh penggunaan

### contoh 1: create dan write file

```c
int fd = vfs_open("/test.txt", O_WRONLY | O_CREAT);
if (fd >= 0) {
    vfs_write(fd, "hello\n", 6);
    vfs_close(fd);
}
```

### contoh 2: read file

```c
int fd = vfs_open("/test.txt", O_RDONLY);
if (fd >= 0) {
    char buf[100];
    int n = vfs_read(fd, buf, sizeof(buf));
    if (n > 0) {
        buf[n] = '\0';
        printf("content: %s\n", buf);
    }
    vfs_close(fd);
}
```

### contoh 3: list directory

```c
struct dirent entry;
int ret = vfs_readdir_init("/home");
while (vfs_readdir_next(&entry) == 0) {
    printf("%s (type: %d)\n", entry.name, entry.type);
}
vfs_readdir_close();
```

---

## troubleshooting

### file gak bisa dibuka

- cek path valid
- cek file exists
- cek permission
- cek filesystem mounted

### read/write error

- cek file descriptor valid
- cek buffer size
- cek disk space
- cek file corruption

### directory operations gagal

- cek path valid
- cek directory exists (buat delete)
- cek directory kosong (buat delete)
- cek parent directory exists (buat create)

---

selanjutnya: [struktur filesystem →](structure.md)
