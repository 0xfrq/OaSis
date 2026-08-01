---
layout: default
title: filesystem
---

# Filesystem (OAFS)

OAFS is custom inode-based filesystem for OaSis. all code in `src/kernel/fs/VFS.c` (many lines) with header in `include/VFS.h`.

## On-disk layout

disk image (`disk.img`) 4mb with 8192 block @ .

```text
block 0-127: reserved (boot sector dll)
block 128: superblock
block 129-128+N: inode table (N = ceil(1024 / (512/64)) = ceil(1024/8) = 128 block)
block sisanya: data blocks
```

## Superblock (block 128)

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

magic number 0x0AF6 dicek when boot. if not cocok, filesystem di-format ulang.

## Inode

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

total inode: 1024. one block can muat 512/64 = 8 inode. inode table butuh 128 block.

## Directory entry

```c
typedef struct {
 uint32_t inode_number; // 
 char name[MAX_FILENAME_LENGTH]; // 
} dir_entry_t;
```

sizeof = . one block can muat 512/36 = entry.

with multi-block support, directory can memiliki 12 block = 168 entries max.

## Block bitmap

bitmap statis `block_bitmap[MAX_data_BLOCKS / 8 + 1]`. tiap bit = 1 data block.

function:
- `bitmap_set(idx)` -> set bit
- `bitmap_clear(idx)` -> clear bit
- `bitmap_test(idx)` -> test bit
- `rebuild_block_bitmap()` -> scan all inode, set bit for each block used

## Key algorithm: get_block_ptr

helper function to get the block number for a file offset.

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

## Key algorithm: set_block_ptr

same with get_block_ptr, tapi alloc block if not yet ada.

```c
static int set_block_ptr(inode_t *in, uint32_t blk_idx) {
 if (blk_idx < 12) {
 if (in->direct[blk_idx] != 0) return (int)in->direct[blk_idx];
 int nb = VFS_alloc_block();
 if (nb < 0) return -1;
 in->direct[blk_idx] = (uint32_t)nb;
 return nb;
 }

 /* indirect block */
 uint32_t ind_idx = blk_idx - 12;
 if (ind_idx >= 128) return -1;

 if (in->indirect == 0) {
 int nb = VFS_alloc_block();
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
 int nb = VFS_alloc_block();
 if (nb < 0) return -1;
 block_ptrs[ind_idx] = (uint32_t)nb;
 write_block(in->indirect, ind_buf);
 }

 return (int)block_ptrs[ind_idx];
}
```

## VFS_read

```c
int VFS_read(int fd, char *buf, uint32_t count) {
 inode_t *in = &VFS.inodes[open_files[fd].inode_number];
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

## VFS_write

same with read, tapi:
1. alloc block if blk_idx >= blk that already ada
2. update inode->size if offset menulis > size
3. save inode and superblock after complete

## VFS_unlink

order:
1. resolve path -> validasi file exists
2. remove the directory entry first (atomic operation)
3. if successful, free all blocks (direct + indirect)
4. free inode
5. save inode table + superblock

if dir_remove_child failed, not ada that ke-free (safe).

## VFS_mkdir

1. alloc inode + 1 block for directory data
2. set type = inode_TYPE_DIR, parent_inode = parent
3. add the child entry to the parent directory
4. save inode

## VFS_resolve_path

parse path component by component.

```text
path = "/home/user/file.txt"
-> cur = 0 (root)
-> part = "home" -> dir_find_child(0, "home") -> cur = home_inode
-> part = "user" -> dir_find_child(home_inode, "user") -> cur = user_inode
-> part = "file.txt" -> dir_find_child(user_inode, "file.txt") -> cur = file_inode
return file_inode
```

support absolute (`/path`), relative (`path`), `.`, `..`.

## Directory operations

### Dir_read_entries

read all directory blocks (up to 12 block), concatenate entries into the buffer.

### Dir_write_entries

write entries to multiple blocks. allocate a new block when needed.

### Dir_add_child

1. read entries -> cek if name already ada -> return -1
2. if not yet penuh, tambah entry
3. write entries

### Dir_remove_child

1. read entries -> find name
2. shift following entries forward
3. write entries

## Fd layer (src/kernel/fs/fd.c)

file descriptor table abstraction above VFS. each task has fd_table sendiri.

```c
typedef struct {
 int type; // FD_TYPE_NONE, FD_TYPE_CONSOLE, FD_TYPE_FILE, FD_TYPE_PIPE
 int flags; // FD_FLAG_READ, FD_FLAG_WRITE, FD_FLAG_APPEND
 int ref_count;
 uint32_t offset;
 union {
 uint32_t VFS_fd; // for file type
 uint32_t pipe_id; // for pipe type
 } data;
} fd_entry_t;
```

### Fd_open

convert POSIX flags (`O_RDONLY=0`, `O_WRONLY=1`, `O_RDWR=2`) to VFS flags:
```c
if (flags & O_RDWR == O_RDWR) VFS_flags = VFS_O_READ | VFS_O_WRITE;
else if (flags & O_WRONLY) VFS_flags = VFS_O_WRITE;
else VFS_flags = VFS_O_READ;
```

call `VFS_open(path, VFS_flags)`, setup fd entry with VFS_fd that di-return.

### Default fd

| fd | type | flags | deskripsi |
|----|------|-------|-----------|
| 0 | console | read | stdin |
| 1 | console | write | stdout |
| 2 | console | write | stderr |

### Kernel_fd_table

fd_get_current_table() -> if current_task is NULL or current_task->fd_table is NULL, return `&kernel_fd_table`. this kernel code can still access file descriptors without a task context.

### Special device paths

fd_open deteksi path special:
- `/dev/console`, `/dev/tty` -> fd type console, read+write
- `/dev/stdin` -> fd type console, read
- `/dev/stdout`, `/dev/stderr` -> fd type console, write
- other -> VFS_open for file biasa
