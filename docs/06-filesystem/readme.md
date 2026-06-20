---
layout: default
title: Filesystem
---

# 06. Filesystem (OAFS)

## OAFS Architecture

OAFS (Oasis File System) is a custom inode-based filesystem.

### On-Disk Layout

```
[Block 0: Superblock] [Blocks 1..N: Inode Table] [Blocks N+1..8192: Data Blocks]
```

### Superblock
```c
uint32_t magic;         // VFS_MAGIC (0x0AF6)
uint32_t total_blocks;  // 8192
uint32_t total_inodes;  // 1024
uint32_t free_inodes;
uint32_t free_blocks;
```

### Inode Structure
```c
uint32_t type;          // FILE (1), DIR (2), FREE (0)
uint32_t size;          // File size in bytes
uint32_t parent_inode;  // Parent directory inode
uint32_t direct[12];    // Direct block pointers (12 x 512 = 6KB)
uint32_t indirect;      // Indirect block pointer (128 x 512 = 64KB)
uint32_t ctime;         // Creation time
uint32_t mtime;         // Modification time
char name[32];          // File/directory name
```

Total file capacity: 12 direct + 128 indirect = 70KB per file.

### Directory Entries
```c
uint32_t inode_number;  // Inode number
char name[32];           // Entry name
```

Directories use up to 12 data blocks (168 entries max).

## Key Operations

### Open (`vfs_open`)
1. Resolve path via `vfs_resolve_path()`.
2. If not found and `O_CREATE` is set, create the file.
3. Allocate an open file entry (up to 32).

### Read (`vfs_read`)
1. Look up block at file offset via `get_block_ptr()`.
2. For direct blocks (0-11): read from `in->direct[blk_idx]`.
3. For indirect blocks (12+): read indirect block pointer table, then the data block.
4. Copy chunk to user buffer.

### Write (`vfs_write`)
1. Allocate blocks on demand via `set_block_ptr()`.
2. For indirect blocks, alloc the indirect block if needed, then alloc data blocks.
3. Track file size as `max(write_end, current_size)`.

### Unlink (`vfs_unlink`)
1. Resolve path and validate it's a file.
2. Remove directory entry first (atomic).
3. Free all data blocks (direct + indirect).
4. Free inode. Save inode table and superblock.
