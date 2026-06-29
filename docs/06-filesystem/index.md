---
layout: default
title: filesystem
---

# filesystem (oafs)

oafs adalah inode-based filesystem kustom untuk oasis. semua kode di `src/kernel/fs/vfs.c` (~banyak baris) dengan header di `include/vfs.h`.

## on-disk layout

disk image (`disk.img`) 4mb dengan 8192 block @ .

```
block 0-127: reserved (boot sector dll)
block 128: superblock
block 129-128+N: inode table (N = ceil(1024 / (512/64)) = ceil(1024/8) = 128 block)
block sisanya: data blocks
```

## superblock (block 128)

```c
typedef struct {
 uint32_t magic; // VFS_MAGIC = 0x0AF6
 uint32_t total_blocks; // 8192
 uint32_t inode_table_start;
 uint32_t data_start;
 uint32_t total_inodes; // 1024
 uint32_t free_inodes;
 uint32_t free_blocks;
} superblock_t;
```

magic number 0x0AF6 dicek saat boot. kalau tidak cocok, filesystem di-format ulang.

## inode

```c
typedef struct {
 uint32_t type; // 0=free, 1=file, 2=dir
 uint32_t size;
 uint32_t parent_inode;
 uint32_t direct[12]; // 12 direct block pointers (12 * 512 = 6kb)
 uint32_t indirect; // indirect block pointer (128 * 512 = 64kb)
 uint32_t ctime;
 uint32_t mtime;
 char name[32];
} inode_t;
```

max file size: 12 + 128 = 140 block * 512 = 70kb.

total inode: 1024. satu block bisa muat 512/64 = 8 inode. inode table butuh 128 block.

## directory entry

```c
typedef struct {
 uint32_t inode_number; // 
 char name[MAX_FILENAME_LENGTH]; // 
} dir_entry_t;
```

sizeof = . satu block bisa muat 512/36 = entry.

dengan multi-block support, directory bisa memiliki 12 block = 168 entries max.

## block bitmap

bitmap statis `block_bitmap[MAX_DATA_BLOCKS / 8 + 1]`. tiap bit = 1 data block.

fungsi:
- `bitmap_set(idx)` -> set bit
- `bitmap_clear(idx)` -> clear bit
- `bitmap_test(idx)` -> test bit
- `rebuild_block_bitmap()` -> scan semua inode, set bit untuk setiap block yang digunakan

## key algorithm: get_block_ptr

fungsi helper untuk mendapatkan block number dari offset file tertentu.

```c
static int get_block_ptr(inode_t *in, uint32_t blk_idx) {
 if (blk_idx < 12) {
 if (in->direct[blk_idx] == 0) return -1;
 return (int)in->direct[blk_idx];
 }

 /* indirect block */
 if (in->indirect == 0) return -1;

 uint32_t ind_idx = blk_idx - 12;
 if (ind_idx >= 128) return -1;

 uint8_t ind_buf[BLOCK_SIZE];
 read_block(in->indirect, ind_buf);
 uint32_t *block_ptrs = (uint32_t *)ind_buf;
 if (block_ptrs[ind_idx] == 0) return -1;
 return (int)block_ptrs[ind_idx];
}
```

## key algorithm: set_block_ptr

sama dengan get_block_ptr, tapi alloc block kalau belum ada.

```c
static int set_block_ptr(inode_t *in, uint32_t blk_idx) {
 if (blk_idx < 12) {
 if (in->direct[blk_idx] != 0) return (int)in->direct[blk_idx];
 int nb = vfs_alloc_block();
 if (nb < 0) return -1;
 in->direct[blk_idx] = (uint32_t)nb;
 return nb;
 }

 /* indirect block */
 uint32_t ind_idx = blk_idx - 12;
 if (ind_idx >= 128) return -1;

 if (in->indirect == 0) {
 int nb = vfs_alloc_block();
 if (nb < 0) return -1;
 in->indirect = (uint32_t)nb;
 /* zero the indirect block */
 uint8_t zero[BLOCK_SIZE];
 mem_zero(zero, BLOCK_SIZE);
 write_block(in->indirect, zero);
 }

 uint8_t ind_buf[BLOCK_SIZE];
 read_block(in->indirect, ind_buf);
 uint32_t *block_ptrs = (uint32_t *)ind_buf;

 if (block_ptrs[ind_idx] == 0) {
 int nb = vfs_alloc_block();
 if (nb < 0) return -1;
 block_ptrs[ind_idx] = (uint32_t)nb;
 write_block(in->indirect, ind_buf);
 }

 return (int)block_ptrs[ind_idx];
}
```

## vfs_read

```c
int vfs_read(int fd, char *buf, uint32_t count) {
 inode_t *in = &vfs.inodes[open_files[fd].inode_number];
 if (open_files[fd].offset >= in->size) return 0;

 uint32_t done = 0;
 while (done < count) {
 uint32_t off = open_files[fd].offset + done;
 uint32_t blk_idx = off / BLOCK_SIZE;
 uint32_t blk_off = off % BLOCK_SIZE;

 int blk = get_block_ptr(in, blk_idx);
 if (blk < 0) break;

 uint8_t block_buf[BLOCK_SIZE];
 read_block((uint32_t)blk, block_buf);

 uint32_t chunk = BLOCK_SIZE - blk_off;
 if (chunk > (count - done)) chunk = count - done;

 mem_copy(buf + done, block_buf + blk_off, chunk);
 done += chunk;
 }
 open_files[fd].offset += done;
 return (int)done;
}
```

## vfs_write

sama dengan read, tapi:
1. alloc block kalau blk_idx >= blk yang sudah ada
2. update inode->size kalau offset menulis > size
3. save inode dan superblock setelah selesai

## vfs_unlink

urutan:
1. resolve path -> validasi file exists
2. remove directory entry dulu (atomic operation)
3. kalau berhasil, free semua blocks (direct + indirect)
4. free inode
5. save inode table + superblock

kalau dir_remove_child gagal, tidak ada yang ke-free (safe).

## vfs_mkdir

1. alloc inode + 1 block untuk directory data
2. set type = INODE_TYPE_DIR, parent_inode = parent
3. add child entry ke parent directory
4. save inode

## vfs_resolve_path

parse path component by component.

```
path = "/home/user/file.txt"
-> cur = 0 (root)
-> part = "home" -> dir_find_child(0, "home") -> cur = home_inode
-> part = "user" -> dir_find_child(home_inode, "user") -> cur = user_inode
-> part = "file.txt" -> dir_find_child(user_inode, "file.txt") -> cur = file_inode
return file_inode
```

support absolute (`/path`), relative (`path`), `.`, `..`.

## directory operations

### dir_read_entries

baca semua block directory (up to 12 block), concatenate entries ke buffer.

### dir_write_entries

menulis entries ke multiple block. alloc block baru kalau perlu.

### dir_add_child

1. read entries -> cek kalau name sudah ada -> return -1
2. kalau belum penuh, tambah entry
3. write entries

### dir_remove_child

1. read entries -> cari name
2. shift entries setelahnya ke depan
3. write entries

## fd layer (src/kernel/fs/fd.c)

fd table abstraction di atas vfs. setiap task memiliki fd_table sendiri.

```c
typedef struct {
 int type; // FD_TYPE_NONE, FD_TYPE_CONSOLE, FD_TYPE_FILE, FD_TYPE_PIPE
 int flags; // FD_FLAG_READ, FD_FLAG_WRITE, FD_FLAG_APPEND
 int ref_count;
 uint32_t offset;
 union {
 uint32_t vfs_fd; // for file type
 uint32_t pipe_id; // for pipe type
 } data;
} fd_entry_t;
```

### fd_open

konversi posix flags (`O_RDONLY=0`, `O_WRONLY=1`, `O_RDWR=2`) ke vfs flags:
```c
if (flags & O_RDWR == O_RDWR) vfs_flags = VFS_O_READ | VFS_O_WRITE;
else if (flags & O_WRONLY) vfs_flags = VFS_O_WRITE;
else vfs_flags = VFS_O_READ;
```

call `vfs_open(path, vfs_flags)`, setup fd entry dengan vfs_fd yang di-return.

### default fd

| fd | type | flags | deskripsi |
|----|------|-------|-----------|
| 0 | console | read | stdin |
| 1 | console | write | stdout |
| 2 | console | write | stderr |

### kernel_fd_table

fd_get_current_table() -> kalau current_task NULL atau current_task->fd_table NULL, return `&kernel_fd_table`. ini biaya kernel code tetap bisa akses fd meskipun tidak ada task context.

### special device paths

fd_open deteksi path khusus:
- `/dev/console`, `/dev/tty` -> fd type console, read+write
- `/dev/stdin` -> fd type console, read
- `/dev/stdout`, `/dev/stderr` -> fd type console, write
- lainnya -> vfs_open untuk file biasa
