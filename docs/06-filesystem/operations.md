# operasi filesystem

dokumentasi ini membahas operasi-operasi file di oafs.

## daftar isi

- [overview](#overview)
- [path resolution](#path-resolution)
- [open file](#open-file)
- [read file](#read-file)
- [write file](#write-file)
- [create file](#create-file)
- [delete file](#delete-file)
- [seek](#seek)

---

## overview

semua operasi file di OaSis lewat vfs layer. aplikasi tidak perlu tahu detail filesystem.

### flow umum

```text
aplikasi call vfs_open("/path/to/file")
  ↓
vfs resolve path (cari inode)
  ↓
vfs alloc open file descriptor
  ↓
return fd ke aplikasi
  ↓
aplikasi menggunakan fd untuk read/write
  ↓
aplikasi call vfs_close(fd)
```

## path resolution

**path resolution** adalah proses convert path string jadi inode number.

### contoh

```text
/home/user/file.txt

1. mulai dari root (inode 0)
2. cari "home" di root directory -> inode 2
3. cari "user" di inode 2 -> inode 5
4. cari "file.txt" di inode 5 -> inode 12
5. return inode 12
```

### implementasi

```c
int resolve_path(const char *path, uint32_t *inode_out) {
    uint32_t current_inode = 0;  // root
    
    // skip leading /
    if (path[0] == '/') path++;
    
    // parse each component
    char component[256];
    while (*path) {
        // extract component
        int i = 0;
        while (*path && *path != '/') {
            component[i++] = *path++;
        }
        component[i] = '\0';
        
        // find in current directory
        int found = dir_find_entry(current_inode, component, &entry);
        if (!found) return -1;
        
        current_inode = entry.inode;
        
        if (*path == '/') path++;
    }
    
    *inode_out = current_inode;
    return 0;
}
```

## open file

### flags

| flag | nilai | deskripsi |
|------|-------|-----------|
| `VFS_O_READ` | 0x01 | buka untuk baca |
| `VFS_O_WRITE` | 0x02 | buka untuk tulis |
| `VFS_O_CREATE` | 0x04 | membuat kalau belum ada |
| `VFS_O_TRUNC` | 0x08 | truncate ke 0 length |
| `VFS_O_APPEND` | 0x10 | tulis di ujung file |

### implementasi

```c
int vfs_open(const char *path, uint32_t flags) {
    uint32_t inode_num;
    int found = resolve_path(path, &inode_num) == 0;
    
    if (!found && !(flags & VFS_O_CREATE)) {
        return -1;  // file not found
    }
    
    if (!found) {
        // create new file
        inode_num = create_file(path);
    }
    
    // find free slot in open_files table
    int fd = find_free_slot();
    
    // setup open file
    open_files[fd].inode = inode_num;
    open_files[fd].offset = 0;
    open_files[fd].flags = flags;
    
    if (flags & VFS_O_TRUNC) {
        truncate_file(inode_num);
    }
    
    if (flags & VFS_O_APPEND) {
        open_files[fd].offset = inode_table[inode_num].size;
    }
    
    return fd;
}
```

## read file

### implementasi

```c
int vfs_read(int fd, void *buf, uint32_t size) {
    if (!is_valid_fd(fd)) return -1;
    
    uint32_t inode_num = open_files[fd].inode;
    inode_t *inode = &inode_table[inode_num];
    
    // adjust size if reading past end
    if (open_files[fd].offset + size > inode->size) {
        size = inode->size - open_files[fd].offset;
    }
    
    uint32_t bytes_read = 0;
    uint32_t offset = open_files[fd].offset;
    
    while (bytes_read < size) {
        // which block?
        uint32_t block_index = offset / 512;
        uint32_t block_offset = offset % 512;
        
        // get physical block
        uint32_t phys_block = get_block(inode, block_index);
        
        // read sector
        uint8_t sector[512];
        ata_read_sector(phys_block, sector);
        
        // copy to buffer
        uint32_t to_copy = min(512 - block_offset, size - bytes_read);
        memcpy(buf + bytes_read, sector + block_offset, to_copy);
        
        bytes_read += to_copy;
        offset += to_copy;
    }
    
    open_files[fd].offset = offset;
    return bytes_read;
}
```

## write file

### implementasi

```c
int vfs_write(int fd, const void *buf, uint32_t size) {
    if (!is_valid_fd(fd)) return -1;
    
    uint32_t inode_num = open_files[fd].inode;
    inode_t *inode = &inode_table[inode_num];
    
    uint32_t bytes_written = 0;
    uint32_t offset = open_files[fd].offset;
    
    while (bytes_written < size) {
        uint32_t block_index = offset / 512;
        uint32_t block_offset = offset % 512;
        
        // allocate block if needed
        uint32_t phys_block = get_block(inode, block_index);
        if (phys_block == 0) {
            phys_block = alloc_block();
            set_block(inode, block_index, phys_block);
        }
        
        // read-modify-write if partial
        uint8_t sector[512];
        if (block_offset != 0 || size - bytes_written < 512) {
            ata_read_sector(phys_block, sector);
        }
        
        uint32_t to_copy = min(512 - block_offset, size - bytes_written);
        memcpy(sector + block_offset, buf + bytes_written, to_copy);
        
        ata_write_sector(phys_block, sector);
        
        bytes_written += to_copy;
        offset += to_copy;
    }
    
    // update inode size
    if (offset > inode->size) {
        inode->size = offset;
    }
    
    open_files[fd].offset = offset;
    return bytes_written;
}
```

## create file

```c
int vfs_create(const char *path) {
    // parse path to get parent dir and filename
    char parent_path[256], filename[256];
    split_path(path, parent_path, filename);
    
    // resolve parent directory
    uint32_t parent_inode;
    if (resolve_path(parent_path, &parent_inode) != 0) {
        return -1;  // parent not found
    }
    
    // allocate new inode
    uint32_t new_inode = alloc_inode();
    inode_table[new_inode].mode = INODE_FILE;
    inode_table[new_inode].size = 0;
    
    // add entry to parent directory
    dir_add_entry(parent_inode, filename, new_inode);
    
    // update superblock
    superblock.free_inodes--;
    
    return 0;
}
```

## delete file

```c
int vfs_delete(const char *path) {
    uint32_t inode_num;
    if (resolve_path(path, &inode_num) != 0) {
        return -1;  // not found
    }
    
    // free all data blocks
    inode_t *inode = &inode_table[inode_num];
    for (int i = 0; i < 12; i++) {
        if (inode->blocks[i]) {
            free_block(inode->blocks[i]);
        }
    }
    
    // free inode
    free_inode(inode_num);
    
    // remove from parent directory
    remove_dir_entry(path);
    
    // update superblock
    superblock.free_inodes++;
    
    return 0;
}
```

## seek

```c
int vfs_seek(int fd, int32_t offset, int whence) {
    if (!is_valid_fd(fd)) return -1;
    
    uint32_t inode_num = open_files[fd].inode;
    uint32_t size = inode_table[inode_num].size;
    
    int32_t new_offset;
    
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = open_files[fd].offset + offset;
            break;
        case SEEK_END:
            new_offset = size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_offset < 0) return -1;
    
    open_files[fd].offset = new_offset;
    return new_offset;
}
```

---

**kembali ke:** [filesystem →](readme.md)
