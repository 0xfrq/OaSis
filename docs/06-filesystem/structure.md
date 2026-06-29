# struktur filesystem

dokumentasi ini membahas detail struktur oafs di disk.

## daftar isi

- [overview](#overview)
- [boot sector](#boot-sector)
- [superblock](#superblock)
- [block bitmap](#block-bitmap)
- [inode bitmap](#inode-bitmap)
- [inode table](#inode-table)
- [data blocks](#data-blocks)
- [directory structure](#directory-structure)

---

## overview

oafs divide disk jadi beberapa region:

```
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

## boot sector

**boot sector** adalah block pertama di disk (block 0).

### fungsi

- tempat bootloader code
- signature (0xAA55 di offset 510-511)
- partition table (opsional)

### struktur

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

**catatan:** OaSis tidak menggunakan semua field ini, hanya untuk compatibility.

## superblock

**superblock** menyimpan metadata filesystem.

### lokasi

block 1 (setelah boot sector)

### struktur

```c
struct superblock {
    uint32_t magic;              // 0x4F414653 ("OAFS")
    uint32_t version;            // filesystem version
    uint32_t total_blocks;       // total blocks di disk
    uint32_t free_blocks;        // blocks yang free
    uint32_t total_inodes;       // total inodes
    uint32_t free_inodes;        // inodes yang free
    uint32_t block_size;         // 512 bytes
    uint32_t blocks_per_group;   // blocks per group
    uint32_t bitmap_start;       // start block dari block bitmap
    uint32_t inode_bitmap_start; // start block dari inode bitmap
    uint32_t inode_table_start;  // start block dari inode table
    uint32_t data_start;         // start block dari data blocks
    uint32_t root_inode;         // inode number dari root directory
    uint32_t reserved[20];       // buat future use
} __attribute__((packed));
```

### field penting

**magic number**: `0x4F414653` ("OAFS" dalam ASCII)
- digunakan untuk verify ini oafs filesystem

**total_blocks**: jumlah total block di disk
- termasuk boot, superblock, bitmap, inode, data

**free_blocks**: jumlah block yang masih free
- di-update setiap alloc/free block

**root_inode**: inode number dari root directory
- biasanya 0 atau 1

## block bitmap

**block bitmap** track block mana yang free/used.

### lokasi

mulai dari `bitmap_start` (biasanya block 2)

### struktur

```
1 bit = 1 block
bit 0 = block 0, bit 1 = block 1, dst

0 = free
1 = used
```

### ukuran

```
bitmap_size = (total_blocks + 7) / 8 bytes
bitmap_blocks = (bitmap_size + 511) / 512
```

**contoh:** disk 10 MB = 20480 blocks
```
bitmap_size = (20480 + 7) / 8 = 2560 bytes
bitmap_blocks = (2560 + 511) / 512 = 6 blocks
```

### operasi

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

## inode bitmap

**inode bitmap** track inode mana yang free/used.

### lokasi

setelah block bitmap

### struktur

sama kayak block bitmap, tapi track inodes:

```
1 bit = 1 inode
0 = free
1 = used
```

### ukuran

```
bitmap_size = (total_inodes + 7) / 8 bytes
bitmap_blocks = (bitmap_size + 511) / 512
```

## inode table

**inode table** menyimpan semua inode.

### lokasi

setelah inode bitmap

### struktur inode

```c
struct inode {
    uint16_t mode;           // file type + permissions
    uint16_t uid;            // user id
    uint16_t gid;            // group id
    uint32_t size;           // file size dalam bytes
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

### file types

```c
#define INODE_FILE      0x8000  // regular file
#define INODE_DIRECTORY 0x4000  // directory
#define INODE_SYMLINK   0x2000  // symbolic link
```

### block pointers

**direct blocks (12 pointers)**:
- langsung point ke data block
- max: 12 * 512 = 6144 bytes

**single indirect**:
- point ke block yang isinya pointers
- max: (512/4) * 512 = 65536 bytes

**double indirect**:
- point ke block yang isinya single indirect pointers
- max: (512/4) * 65536 = 8388608 bytes

**triple indirect**:
- point ke block yang isinya double indirect pointers
- max: (512/4) * 8388608 = 1073741824 bytes

**total max file size**: ~1 GB

## data blocks

**data blocks** adalah region dimana actual file data disimpen.

### lokasi

setelah inode table sampai end of disk

### ukuran

```
data_blocks = total_blocks - (1 + 1 + bitmap_blocks + inode_table_blocks)
```

### block types

**file data block**:
- menyimpan actual file content
- 512 bytes per block

**directory block**:
- menyimpan directory entries
- format khusus (lihat di bawah)

**indirect block**:
- menyimpan block pointers (4 bytes each)
- 512 / 4 = 128 pointers per block

## directory structure

directory adalah special file yang isinya list of entries.

### directory entry

```c
struct dirent {
    uint32_t inode;      // inode number
    uint8_t name_len;    // length of name
    char name[255];      // file name (null-terminated)
} __attribute__((packed));
```

**ukuran**: minimal 6 bytes, maksimal 260 bytes

### contoh directory

```
/home/user:
  entry 1: inode=10, name="."
  entry 2: inode=5,  name=".."
  entry 3: inode=11, name="documents"
  entry 4: inode=12, name="file.txt"
```

### special entries

**"."**: current directory
**".."**: parent directory

### operasi directory

**add entry:**
```c
int dir_add_entry(int dir_inode, const char *name, int file_inode) {
    // find empty slot atau allocate new block
    // write dirent
    // update directory size
}
```

**remove entry:**
```c
int dir_remove_entry(int dir_inode, const char *name) {
    // find entry
    // mark as deleted (atau compact)
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

## contoh: create file

```
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

**kembali ke:** [filesystem →](readme.md) | **selanjutnya:** [operations →](operations.md)
