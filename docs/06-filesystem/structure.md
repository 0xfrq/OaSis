# Structure filesystem

this page explains details of the OAFS disk structure.

## Contents

- [overview](#overview)
- [boot sector](#boot-sector)
- [superblock](#superblock)
- [block bitmap](#block-bitmap)
- [inode bitmap](#inode-bitmap)
- [inode table](#inode-table)
- [data blocks](#data-blocks)
- [directory structure](#directory-structure)

---

## Overview

OAFS divide disk jadi beberapa region:

```text
┌─────────────────────────────────────┐
│  boot sector (512 bytes)            │  block 0
├─────────────────────────────────────┤
│  superblock (512 bytes)             │  block 1
├─────────────────────────────────────┤
│  block bitmap (variable)            │  block 2 - n
├─────────────────────────────────────┤
│  inode bitmap (variable)            │  block n+1 - m
├─────────────────────────────────────┤
│  inode table (variable)             │  block m+1 - p
├─────────────────────────────────────┤
│  data blocks (rest of disk)         │  block p+1 - end
└─────────────────────────────────────┘
```

## Boot sector

**boot sector** is first block on disk (block 0).

### Function

- tempat bootloader code
- signature (0xAA55 at offset 510-511)
- partition table (opsional)

### Structure

```c
struct boot_sector {
    uint8_t jmp[3];           // jump instruction
    char oem_name[8];         // "OAFS1.0"
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_type;
    uint16_t fat_size;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t large_sectors;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t code[448];        // bootloader code
    uint16_t boot_signature;  // 0xAA55
} __attribute__((packed));
```

**note:** OaSis not using all field this, only for compatibility.

## Superblock

**superblock** store metadata filesystem.

### Lokasi

block 1 (after boot sector)

### Structure

```c
struct superblock {
    uint32_t magic;              // 0x4F414653 ("OAFS")
    uint32_t version;            // filesystem version
    uint32_t total_blocks;       // total blocks on disk
    uint32_t free_blocks;        // free blocks
    uint32_t total_inodes;       // total inodes
    uint32_t free_inodes;        // free inodes
    uint32_t block_size;         // 512 bytes
    uint32_t blocks_per_group;   // blocks per group
    uint32_t bitmap_start;       // start block of the block bitmap
    uint32_t inode_bitmap_start; // start block of the inode bitmap
    uint32_t inode_table_start;  // start block of the inode table
    uint32_t data_start;         // start block of the data blocks
    uint32_t root_inode;         // inode number of the root directory
    uint32_t reserved[20];       // buat future use
} __attribute__((packed));
```

### Field penting

**magic number**: `0x4F414653` ("OAFS" in ASCII)
- used for verify this OAFS filesystem

**total_blocks**: total number of disk blocks
- includes the boot block, superblock, bitmap, inode table, and data

**free_blocks**: number block that still free
- di-update each alloc/free block

**root_inode**: inode number from root directory
- usually 0 or 1

## Block bitmap

**block bitmap** track block mana free/used.

### Lokasi

start from `bitmap_start` (usually block 2)

### Structure

```text
1 bit = 1 block
bit 0 = block 0, bit 1 = block 1, dst

0 = free
1 = used
```

### Ukuran

```text
bitmap_size = (total_blocks + 7) / 8 bytes
bitmap_blocks = (bitmap_size + 511) / 512
```

**example:** disk 10 MB = 20480 blocks
```text
bitmap_size = (20480 + 7) / 8 = 2560 bytes
bitmap_blocks = (2560 + 511) / 512 = 6 blocks
```

### Operations

**alloc block:**
```c
int alloc_block(void) {
    for (int i = 0; i < total_blocks; i++) {
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            superblock.free_blocks--;
            return i;
        }
    }
    return -1;  // out of space
}
```

**free block:**
```c
void free_block(int block_num) {
    bitmap_clear(bitmap, block_num);
    superblock.free_blocks++;
}
```

## Inode bitmap

**inode bitmap** track inode mana free/used.

### Lokasi

after block bitmap

### Structure

same kayak block bitmap, tapi track inodes:

```text
1 bit = 1 inode
0 = free
1 = used
```

### Ukuran

```text
bitmap_size = (total_inodes + 7) / 8 bytes
bitmap_blocks = (bitmap_size + 511) / 512
```

## Inode table

**inode table** store all inode.

### Lokasi

after inode bitmap

### Structure inode

```c
struct inode {
    uint16_t mode;           // file type + permissions
    uint16_t uid;            // user id
    uint16_t gid;            // group id
    uint32_t size;           // file size in bytes
    uint32_t atime;          // access time
    uint32_t mtime;          // modification time
    uint32_t ctime;          // creation time
    uint32_t blocks[12];     // direct block pointers
    uint32_t indirect;       // single indirect block
    uint32_t double_indirect; // double indirect block
    uint32_t triple_indirect; // triple indirect block
} __attribute__((packed));
```

**ukuran inode**: 64 bytes

**inodes per block**: 512 / 64 = 8 inodes

### File types

```c
# Define INODE_FILE      0x8000  // regular file
# Define INODE_DIRECTORY 0x4000  // directory
# Define INODE_SYMLINK   0x2000  // symbolic link
```

### Block pointers

**direct blocks (12 pointers)**:
- point directly to a data block
- max: 12 * 512 = 6144 bytes

**single indirect**:
- point to a block containing pointers
- max: (512/4) * 512 = 65536 bytes

**double indirect**:
- point to a block containing single-indirect pointers
- max: (512/4) * 65536 = 8388608 bytes

**triple indirect**:
- point to a block containing double-indirect pointers
- max: (512/4) * 8388608 = 1073741824 bytes

**total max file size**: ~1 GB

## Data blocks

**data blocks** is region dimana actual file data disimpen.

### Lokasi

after inode table until end of disk

### Ukuran

```text
data_blocks = total_blocks - (1 + 1 + bitmap_blocks + inode_table_blocks)
```

### Block types

**file data block**:
- store actual file content
- 512 bytes per block

**directory block**:
- store directory entries
- special format (see below)

**indirect block**:
- store block pointers (4 bytes each)
- 512 / 4 = 128 pointers per block

## Directory structure

directory is special file that isinya list of entries.

### Directory entry

```c
struct dirent {
    uint32_t inode;      // inode number
    uint8_t name_len;    // length of name
    char name[255];      // file name (null-terminated)
} __attribute__((packed));
```

**ukuran**: minimal 6 bytes, maksimal 260 bytes

### Example directory

```text
/home/user:
  entry 1: inode=10, name="."
  entry 2: inode=5,  name=".."
  entry 3: inode=11, name="documents"
  entry 4: inode=12, name="file.txt"
```

### Special entries

**"."**: current directory
**".."**: parent directory

### Operations directory

**add entry:**
```c
int dir_add_entry(int dir_inode, const char *name, int file_inode) {
    // find an empty slot or allocate a new block
    // write dirent
    // update directory size
}
```

**remove entry:**
```c
int dir_remove_entry(int dir_inode, const char *name) {
    // find entry
    // mark as deleted (or compact)
    // update directory size
}
```

**find entry:**
```c
int dir_find_entry(int dir_inode, const char *name, struct dirent *entry) {
    // iterate all entries
    // compare name
    // return inode if found
}
```

---

## Example: create file

```text
user: touch /home/test.txt

1. resolve path "/home/test.txt"
   - root inode (0) -> "/home" -> inode 5
   - inode 5 -> "test.txt" -> not found

2. allocate new inode
   - find free inode in inode bitmap
   - mark as used
   - initialize inode (mode=FILE, size=0)

3. add directory entry
   - open /home directory (inode 5)
   - add entry: name="test.txt", inode=new_inode

4. update superblock
   - free_inodes--
   - write superblock to disk

5. done!
```

---

**back to:** [filesystem →](readme.md) | **next:** [operations →](operations.md)
