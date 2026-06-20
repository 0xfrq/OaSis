---
layout: default
title: filesystem
---

# filesystem (oafs)

oafs adalah inode-based filesystem kustom di `src/kernel/fs/vfs.c`.

## on-disk layout

```
[block 128: superblock] [129..: inode table] [..8192: data blocks]
```

block size: 512 byte. total block: 8192 -> 4mb filesystem.

superblock:
- magic (0x0AF6)
- free_inodes, free_blocks
- total_inodes (1024), total_blocks

inode:
- type (0=free, 1=file, 2=dir)
- size
- parent_inode
- direct[12] -> 12 * 512 = 6kb
- indirect -> 128 * 512 = 64kb
- total max file: 70kb
- name[32]

directory entry:
- inode_number (4 byte)
- name[32]
- max 14 entry per block -> 168 entries dengan multi-block

## key algorithm

### get_block_ptr(in, blk_idx)

ambil block number untuk offset tertentu:
- kalo blk_idx < 12 -> direct[blk_idx]
- kalo blk_idx >= 12 -> baca indirect block, ambil ptr[blk_idx - 12]

### set_block_ptr(in, blk_idx)

sama kaya get_block_ptr, tapi alloc block kalo belum ada.

### dir_read_entries

baca semua block directory (up to 12 block), concatenate entries.

### dir_write_entries

tulis entries ke multiple block sesuai kebutuhan.

## fd layer

di `src/kernel/fs/fd.c`. fd layer duduk di atas vfs.

- konversi posix flags ke vfs flags
- track file offset, flags, ref_count
- fd 0/1/2 = console (stdin/stdout/stderr)

## shell commands terkait

```
touch <file>   -> vfs_create()
cat <file>     -> vfs_open(O_RDONLY) + vfs_read()
write <f> <t>  -> vfs_open(O_WRITE|O_CREATE|O_TRUNC) + vfs_write()
ls [path]      -> vfs_list()
cd <path>      -> vfs_chdir()
rm <file>      -> vfs_unlink()
mkdir <p>      -> vfs_mkdir()
rmdir <p>      -> vfs_rmdir()
```
