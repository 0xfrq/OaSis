#include "vfs.h"
#include "block.h"
#include "vga.h"
#include "string.h"
#include "timer.h"
#include <stdint.h>

#define NULL ((void*)0)

#define FS_BLOCK_START 128
#define FS_TOTAL_BLOCKS 8192
#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(inode_t))
#define INODE_TABLE_BLOCKS ((MAX_INODES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK)
#define DATA_BLOCK_START (FS_BLOCK_START + 1 + INODE_TABLE_BLOCKS)
#define MAX_DATA_BLOCKS (FS_TOTAL_BLOCKS - (1 + INODE_TABLE_BLOCKS))

static vfs_state_t vfs;
static uint8_t block_bitmap[MAX_DATA_BLOCKS / 8 + 1];
static dir_entry_t dir_scratch[BLOCK_SIZE / sizeof(dir_entry_t)];

static void mem_zero(void *ptr, uint32_t len) {
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < len; i++) p[i] = 0;
}

static void mem_copy(void *dst, const void *src, uint32_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < len; i++) d[i] = s[i];
}

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static void str_copy(char *dst, const char *src, uint32_t max_len) {
    uint32_t i = 0;
    if (max_len == 0) return;
    while (src && src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static int read_block(uint32_t abs_block, void *buf) {
    return block_read(abs_block, (uint8_t *)buf);
}

static int write_block(uint32_t abs_block, const void *buf) {
    return block_write(abs_block, (const uint8_t *)buf);
}

static int save_superblock(void) {
    uint8_t buf[BLOCK_SIZE];
    mem_zero(buf, BLOCK_SIZE);
    mem_copy(buf, &vfs.superblock, sizeof(superblock_t));
    return write_block(FS_BLOCK_START, buf);
}

static int load_superblock(void) {
    uint8_t buf[BLOCK_SIZE];
    if (read_block(FS_BLOCK_START, buf) != 0) return -1;
    mem_copy(&vfs.superblock, buf, sizeof(superblock_t));
    return 0;
}

static int save_inode(uint32_t inode_num) {
    if (inode_num >= MAX_INODES) return -1;

    uint32_t block_index = inode_num / INODES_PER_BLOCK;
    uint32_t offset = inode_num % INODES_PER_BLOCK;
    uint32_t abs_block = FS_BLOCK_START + 1 + block_index;

    uint8_t buf[BLOCK_SIZE];
    if (read_block(abs_block, buf) != 0) return -1;

    inode_t *table = (inode_t *)buf;
    table[offset] = vfs.inodes[inode_num];

    return write_block(abs_block, buf);
}

static int load_inode_table(void) {
    uint32_t inode_num = 0;
    for (uint32_t b = 0; b < INODE_TABLE_BLOCKS; b++) {
        uint8_t buf[BLOCK_SIZE];
        if (read_block(FS_BLOCK_START + 1 + b, buf) != 0) return -1;
        inode_t *table = (inode_t *)buf;

        for (uint32_t i = 0; i < INODES_PER_BLOCK && inode_num < MAX_INODES; i++) {
            vfs.inodes[inode_num++] = table[i];
        }
    }
    return 0;
}

static int save_inode_table(void) {
    uint32_t inode_num = 0;
    for (uint32_t b = 0; b < INODE_TABLE_BLOCKS; b++) {
        uint8_t buf[BLOCK_SIZE];
        mem_zero(buf, BLOCK_SIZE);
        inode_t *table = (inode_t *)buf;

        for (uint32_t i = 0; i < INODES_PER_BLOCK && inode_num < MAX_INODES; i++) {
            table[i] = vfs.inodes[inode_num++];
        }

        if (write_block(FS_BLOCK_START + 1 + b, buf) != 0) return -1;
    }
    return 0;
}

static void bitmap_set(uint32_t idx) {
    block_bitmap[idx / 8] |= (1 << (idx % 8));
}

static void bitmap_clear(uint32_t idx) {
    block_bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static int bitmap_test(uint32_t idx) {
    return (block_bitmap[idx / 8] & (1 << (idx % 8))) != 0;
}

static void rebuild_block_bitmap(void) {
    mem_zero(block_bitmap, sizeof(block_bitmap));

    for (uint32_t i = 0; i < MAX_INODES; i++) {
        inode_t *in = &vfs.inodes[i];
        if (in->type == INODE_TYPE_FREE) continue;

        for (int j = 0; j < 12; j++) {
            if (in->direct[j] >= DATA_BLOCK_START && in->direct[j] < FS_BLOCK_START + FS_TOTAL_BLOCKS) {
                uint32_t rel = in->direct[j] - DATA_BLOCK_START;
                if (rel < MAX_DATA_BLOCKS) bitmap_set(rel);
            }
        }
    }
}

static int dir_read_entries(uint32_t dir_inode, dir_entry_t *entries, uint32_t *count) {
    if (dir_inode >= MAX_INODES) return -1;
    inode_t *dir = &vfs.inodes[dir_inode];
    if (dir->type != INODE_TYPE_DIR) return -1;

    if (dir->direct[0] == 0) {
        *count = 0;
        return 0;
    }

    uint8_t buf[BLOCK_SIZE];
    if (read_block(dir->direct[0], buf) != 0) return -1;

    uint32_t max = BLOCK_SIZE / sizeof(dir_entry_t);
    dir_entry_t *disk_entries = (dir_entry_t *)buf;
    uint32_t n = 0;
    for (uint32_t i = 0; i < max; i++) {
        if (disk_entries[i].inode_number != 0 || str_len(disk_entries[i].name) > 0) {
            entries[n++] = disk_entries[i];
        }
    }

    *count = n;
    return 0;
}

static int dir_write_entries(uint32_t dir_inode, dir_entry_t *entries, uint32_t count) {
    if (dir_inode >= MAX_INODES) return -1;
    inode_t *dir = &vfs.inodes[dir_inode];
    if (dir->type != INODE_TYPE_DIR) return -1;

    if (dir->direct[0] == 0) {
        int b = vfs_alloc_block();
        if (b < 0) return -1;
        dir->direct[0] = (uint32_t)b;
    }

    uint8_t buf[BLOCK_SIZE];
    mem_zero(buf, BLOCK_SIZE);

    uint32_t max = BLOCK_SIZE / sizeof(dir_entry_t);
    if (count > max) count = max;

    dir_entry_t *disk_entries = (dir_entry_t *)buf;
    for (uint32_t i = 0; i < count; i++) {
        disk_entries[i] = entries[i];
    }

    dir->size = count * sizeof(dir_entry_t);
    dir->mtime = vfs_get_ticks();

    if (write_block(dir->direct[0], buf) != 0) return -1;
    return save_inode(dir_inode);
}

static int dir_find_child(uint32_t dir_inode, const char *name, uint32_t *child_inode) {
    uint8_t tmp_buf[BLOCK_SIZE];
    dir_entry_t *entries = (dir_entry_t *)tmp_buf;
    uint32_t count = 0;

    if (dir_read_entries(dir_inode, entries, &count) != 0) return -1;

    for (uint32_t i = 0; i < count; i++) {
        if (str_eq(entries[i].name, name)) {
            *child_inode = entries[i].inode_number;
            return 0;
        }
    }

    return -1;
}

static int dir_add_child(uint32_t dir_inode, const char *name, uint32_t child_inode) {
    // Use global scratch buffer to avoid stack overflow
    dir_entry_t *entries = dir_scratch; // reuse global buffer
    uint32_t count = 0;

    if (dir_read_entries(dir_inode, entries, &count) != 0) return -1;
    if (count >= MAX_DIR_ENTRIES) {
        vga_print("mkdir: directory full\n");
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (str_eq(entries[i].name, name)) return -1;
    }

    entries[count].inode_number = child_inode;
    str_copy(entries[count].name, name, MAX_FILENAME_LENGTH);

    return dir_write_entries(dir_inode, entries, count + 1);
}

static int dir_remove_child(uint32_t dir_inode, const char *name) {
    dir_entry_t *entries = dir_scratch; // use global scratch
    uint32_t count = 0;

    if (dir_read_entries(dir_inode, entries, &count) != 0) return -1;

    int found = -1;
    for (uint32_t i = 0; i < count; i++) {
        if (str_eq(entries[i].name, name)) {
            found = (int)i;
            break;
        }
    }
    if (found < 0) return -1;

    for (uint32_t i = (uint32_t)found; i + 1 < count; i++) {
        entries[i] = entries[i + 1];
    }

    return dir_write_entries(dir_inode, entries, count - 1);
}

static int split_parent_child(const char *path, char *parent, char *child) {
    uint32_t len = str_len(path);
    if (len == 0) return -1;

    int last_slash = -1;
    for (uint32_t i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = (int)i;
    }

    if (last_slash < 0) {
        str_copy(parent, ".", MAX_PATH_LENGTH);
        str_copy(child, path, MAX_FILENAME_LENGTH);
        return 0;
    }

    if (last_slash == 0) {
        parent[0] = '/';
        parent[1] = 0;
        str_copy(child, path + 1, MAX_FILENAME_LENGTH);
        return 0;
    }

    uint32_t p = 0;
    for (int i = 0; i < last_slash && p < MAX_PATH_LENGTH - 1; i++) parent[p++] = path[i];
    parent[p] = 0;

    str_copy(child, path + last_slash + 1, MAX_FILENAME_LENGTH);
    return 0;
}

int vfs_alloc_inode(void) {
    for (uint32_t i = 1; i < MAX_INODES; i++) {
        if (vfs.inodes[i].type == INODE_TYPE_FREE) {
            vfs.superblock.free_inodes--;
            return (int)i;
        }
    }
    return -1;
}

void vfs_free_inode(uint32_t inode_num) {
    if (inode_num == 0 || inode_num >= MAX_INODES) return;
    vfs.inodes[inode_num].type = INODE_TYPE_FREE;
    vfs.superblock.free_inodes++;
}

int vfs_alloc_block(void) {
    for (uint32_t i = 0; i < MAX_DATA_BLOCKS; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            vfs.superblock.free_blocks--;
            uint32_t abs = DATA_BLOCK_START + i;

            uint8_t zero[BLOCK_SIZE];
            mem_zero(zero, BLOCK_SIZE);
            write_block(abs, zero);

            return (int)abs;
        }
    }
    return -1;
}

void vfs_free_block(uint32_t block_num) {
    if (block_num < DATA_BLOCK_START || block_num >= FS_BLOCK_START + FS_TOTAL_BLOCKS) return;
    uint32_t rel = block_num - DATA_BLOCK_START;
    if (rel >= MAX_DATA_BLOCKS) return;

    if (bitmap_test(rel)) {
        bitmap_clear(rel);
        vfs.superblock.free_blocks++;
    }
}

static int format_fs(void) {
    mem_zero(&vfs, sizeof(vfs));

    vfs.superblock.magic = VFS_MAGIC;
    vfs.superblock.total_blocks = FS_TOTAL_BLOCKS;
    vfs.superblock.inode_table_start = FS_BLOCK_START + 1;
    vfs.superblock.data_start = DATA_BLOCK_START;
    vfs.superblock.total_inodes = MAX_INODES;
    vfs.superblock.free_inodes = MAX_INODES - 1;
    vfs.superblock.free_blocks = MAX_DATA_BLOCKS;

    /* Root inode */
    inode_t *root = &vfs.inodes[0];
    root->type = INODE_TYPE_DIR;
    root->size = 0;
    root->parent_inode = 0;
    root->ctime = vfs_get_ticks();
    root->mtime = root->ctime;
    str_copy(root->name, "/", MAX_FILENAME_LENGTH);

    int root_blk = vfs_alloc_block();
    if (root_blk < 0) return -1;
    root->direct[0] = (uint32_t)root_blk;

    /* Create /home, /bin, /tmp */
    int home = vfs_alloc_inode();
    int bin = vfs_alloc_inode();
    int tmp = vfs_alloc_inode();
    if (home < 0 || bin < 0 || tmp < 0) return -1;

    inode_t *h = &vfs.inodes[home];
    inode_t *b = &vfs.inodes[bin];
    inode_t *t = &vfs.inodes[tmp];

    h->type = b->type = t->type = INODE_TYPE_DIR;
    h->parent_inode = b->parent_inode = t->parent_inode = 0;
    h->ctime = b->ctime = t->ctime = vfs_get_ticks();
    h->mtime = b->mtime = t->mtime = h->ctime;
    str_copy(h->name, "home", MAX_FILENAME_LENGTH);
    str_copy(b->name, "bin", MAX_FILENAME_LENGTH);
    str_copy(t->name, "tmp", MAX_FILENAME_LENGTH);

    int hb = vfs_alloc_block();
    int bb = vfs_alloc_block();
    int tb = vfs_alloc_block();
    if (hb < 0 || bb < 0 || tb < 0) return -1;
    h->direct[0] = (uint32_t)hb;
    b->direct[0] = (uint32_t)bb;
    t->direct[0] = (uint32_t)tb;

    if (dir_add_child(0, "home", (uint32_t)home) != 0) return -1;
    if (dir_add_child(0, "bin", (uint32_t)bin) != 0) return -1;
    if (dir_add_child(0, "tmp", (uint32_t)tmp) != 0) return -1;

    if (save_superblock() != 0) return -1;
    if (save_inode_table() != 0) return -1;

    vfs.cwd.inode_number = 0;
    str_copy(vfs.cwd.path, "/", MAX_PATH_LENGTH);
    vfs.initialized = 1;

    vga_print("[+] OAFS formatted and initialized\n");
    return 0;
}

int vfs_init(void) {
    mem_zero(&vfs, sizeof(vfs));

    if (load_superblock() != 0 || vfs.superblock.magic != VFS_MAGIC) {
        vga_print("[*] OAFS not found, formatting...\n");
        return format_fs();
    }

    if (load_inode_table() != 0) {
        vga_print("[-] Failed to load inode table, formatting...\n");
        return format_fs();
    }

    rebuild_block_bitmap();

    vfs.cwd.inode_number = 0;
    str_copy(vfs.cwd.path, "/", MAX_PATH_LENGTH);
    vfs.initialized = 1;

    vga_print("[+] OAFS loaded\n");
    return 0;
}

int vfs_resolve_path(const char *path, uint32_t *inode_out) {
    if (!vfs.initialized || !path || !inode_out) return -1;

    if (str_eq(path, "/")) {
        *inode_out = 0;
        return 0;
    }

    if (str_eq(path, ".")) {
        *inode_out = vfs.cwd.inode_number;
        return 0;
    }

    uint32_t cur = (path[0] == '/') ? 0 : vfs.cwd.inode_number;

    char part[MAX_FILENAME_LENGTH];
    uint32_t pi = 0;

    const char *p = path;
    if (path[0] == '/') p++;

    while (1) {
        if (*p == '/' || *p == 0) {
            if (pi > 0) {
                part[pi] = 0;

                if (str_eq(part, ".")) {
                } else if (str_eq(part, "..")) {
                    cur = vfs.inodes[cur].parent_inode;
                } else {
                    uint32_t child = 0;
                    if (dir_find_child(cur, part, &child) != 0) return -1;
                    cur = child;
                }
                pi = 0;
            }

            if (*p == 0) break;
            p++;
            continue;
        }

        if (pi < MAX_FILENAME_LENGTH - 1) part[pi++] = *p;
        p++;
    }

    *inode_out = cur;
    return 0;
}

int vfs_mkdir(const char *path) {
    char parent[MAX_PATH_LENGTH];
    char name[MAX_FILENAME_LENGTH];

    if (!path || str_len(path) == 0) return -1;
    if (split_parent_child(path, parent, name) != 0) return -1;
    if (str_len(name) == 0) return -1;

    uint32_t pino = 0;
    if (vfs_resolve_path(parent, &pino) != 0) return -1;
    if (vfs.inodes[pino].type != INODE_TYPE_DIR) return -1;

    uint32_t existing = 0;
    if (dir_find_child(pino, name, &existing) == 0) return -1;

    int ino = vfs_alloc_inode();
    if (ino < 0) return -1;

    inode_t *in = &vfs.inodes[ino];
    mem_zero(in, sizeof(inode_t));
    in->type = INODE_TYPE_DIR;
    in->parent_inode = pino;
    in->ctime = vfs_get_ticks();
    in->mtime = in->ctime;
    str_copy(in->name, name, MAX_FILENAME_LENGTH);

    int blk = vfs_alloc_block();
    if (blk < 0) {
        vfs_free_inode((uint32_t)ino);
        return -1;
    }
    in->direct[0] = (uint32_t)blk;

    if (dir_add_child(pino, name, (uint32_t)ino) != 0) {
        vfs_free_block((uint32_t)blk);
        vfs_free_inode((uint32_t)ino);
        return -1;
    }

    save_inode((uint32_t)ino);
    save_superblock();
    return 0;
}

int vfs_create(const char *path) {
    char parent[MAX_PATH_LENGTH];
    char name[MAX_FILENAME_LENGTH];

    if (!path || str_len(path) == 0) return -1;
    if (split_parent_child(path, parent, name) != 0) return -1;
    if (str_len(name) == 0) return -1;

    uint32_t pino = 0;
    if (vfs_resolve_path(parent, &pino) != 0) return -1;
    if (vfs.inodes[pino].type != INODE_TYPE_DIR) return -1;

    uint32_t existing = 0;
    if (dir_find_child(pino, name, &existing) == 0) return -1;

    int ino = vfs_alloc_inode();
    if (ino < 0) return -1;

    inode_t *in = &vfs.inodes[ino];
    mem_zero(in, sizeof(inode_t));
    in->type = INODE_TYPE_FILE;
    in->size = 0;
    in->parent_inode = pino;
    in->ctime = vfs_get_ticks();
    in->mtime = in->ctime;
    str_copy(in->name, name, MAX_FILENAME_LENGTH);

    if (dir_add_child(pino, name, (uint32_t)ino) != 0) {
        vfs_free_inode((uint32_t)ino);
        return -1;
    }

    save_inode((uint32_t)ino);
    save_superblock();
    return 0;
}

int vfs_open(const char *path, uint32_t flags) {
    uint32_t ino = 0;
    if (vfs_resolve_path(path, &ino) != 0) {
        if (flags & VFS_O_CREATE) {
            if (vfs_create(path) != 0) return -1;
            if (vfs_resolve_path(path, &ino) != 0) return -1;
        } else {
            return -1;
        }
    }

    if (vfs.inodes[ino].type != INODE_TYPE_FILE) return -1;

    if (flags & VFS_O_TRUNC) {
        inode_t *in = &vfs.inodes[ino];
        for (int j = 0; j < 12; j++) {
            if (in->direct[j]) {
                vfs_free_block(in->direct[j]);
                in->direct[j] = 0;
            }
        }
        in->size = 0;
        in->mtime = vfs_get_ticks();
        save_inode(ino);
        save_superblock();
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (vfs.open_files[i].ref_count == 0) {
            vfs.open_files[i].inode_number = ino;
            vfs.open_files[i].offset = (flags & VFS_O_APPEND) ? vfs.inodes[ino].size : 0;
            vfs.open_files[i].flags = flags;
            vfs.open_files[i].ref_count = 1;
            return i;
        }
    }

    return -1;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    if (vfs.open_files[fd].ref_count == 0) return -1;

    vfs.open_files[fd].ref_count = 0;
    return 0;
}

int vfs_read(int fd, char *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !buf) return -1;
    if (vfs.open_files[fd].ref_count == 0) return -1;
    if (!(vfs.open_files[fd].flags & VFS_O_READ)) return -1;

    inode_t *in = &vfs.inodes[vfs.open_files[fd].inode_number];
    if (in->type != INODE_TYPE_FILE) return -1;

    if (vfs.open_files[fd].offset >= in->size) return 0;

    uint32_t remaining = in->size - vfs.open_files[fd].offset;
    if (count > remaining) count = remaining;

    uint32_t done = 0;
    while (done < count) {
        uint32_t off = vfs.open_files[fd].offset + done;
        uint32_t blk_idx = off / BLOCK_SIZE;
        uint32_t blk_off = off % BLOCK_SIZE;

        if (blk_idx >= 12 || in->direct[blk_idx] == 0) break;

        uint8_t block_buf[BLOCK_SIZE];
        if (read_block(in->direct[blk_idx], block_buf) != 0) break;

        uint32_t chunk = BLOCK_SIZE - blk_off;
        if (chunk > (count - done)) chunk = count - done;

        for (uint32_t i = 0; i < chunk; i++) buf[done + i] = (char)block_buf[blk_off + i];
        done += chunk;
    }

    vfs.open_files[fd].offset += done;
    return (int)done;
}

int vfs_write(int fd, const char *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !buf) return -1;
    if (vfs.open_files[fd].ref_count == 0) return -1;
    if (!(vfs.open_files[fd].flags & VFS_O_WRITE)) return -1;

    inode_t *in = &vfs.inodes[vfs.open_files[fd].inode_number];
    if (in->type != INODE_TYPE_FILE) return -1;

    uint32_t done = 0;
    while (done < count) {
        uint32_t off = vfs.open_files[fd].offset + done;
        uint32_t blk_idx = off / BLOCK_SIZE;
        uint32_t blk_off = off % BLOCK_SIZE;

        if (blk_idx >= 12) break;

        if (in->direct[blk_idx] == 0) {
            int nb = vfs_alloc_block();
            if (nb < 0) break;
            in->direct[blk_idx] = (uint32_t)nb;
        }

        uint8_t block_buf[BLOCK_SIZE];
        if (read_block(in->direct[blk_idx], block_buf) != 0) break;

        uint32_t chunk = BLOCK_SIZE - blk_off;
        if (chunk > (count - done)) chunk = count - done;

        for (uint32_t i = 0; i < chunk; i++) block_buf[blk_off + i] = (uint8_t)buf[done + i];
        if (write_block(in->direct[blk_idx], block_buf) != 0) break;

        done += chunk;
    }

    vfs.open_files[fd].offset += done;
    if (vfs.open_files[fd].offset > in->size) in->size = vfs.open_files[fd].offset;
    in->mtime = vfs_get_ticks();

    save_inode(vfs.open_files[fd].inode_number);
    save_superblock();

    return (int)done;
}

int vfs_seek(int fd, uint32_t offset) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    if (vfs.open_files[fd].ref_count == 0) return -1;

    vfs.open_files[fd].offset = offset;
    return 0;
}

int vfs_unlink(const char *path) {
    uint32_t ino = 0;
    if (vfs_resolve_path(path, &ino) != 0) return -1;
    if (ino == 0) return -1;

    inode_t *in = &vfs.inodes[ino];
    if (in->type != INODE_TYPE_FILE) return -1;

    for (int i = 0; i < 12; i++) {
        if (in->direct[i]) {
            vfs_free_block(in->direct[i]);
            in->direct[i] = 0;
        }
    }

    char parent[MAX_PATH_LENGTH], name[MAX_FILENAME_LENGTH];
    if (split_parent_child(path, parent, name) != 0) return -1;
    uint32_t pino = 0;
    if (vfs_resolve_path(parent, &pino) != 0) return -1;
    if (dir_remove_child(pino, name) != 0) return -1;

    vfs_free_inode(ino);
    save_inode_table();
    save_superblock();
    return 0;
}

int vfs_rmdir(const char *path) {
    uint32_t ino = 0;
    if (vfs_resolve_path(path, &ino) != 0) return -1;
    if (ino == 0) return -1;

    inode_t *in = &vfs.inodes[ino];
    if (in->type != INODE_TYPE_DIR) return -1;

    uint8_t rmdir_buf[BLOCK_SIZE];
    dir_entry_t *entries = (dir_entry_t *)rmdir_buf;
    uint32_t count = 0;
    if (dir_read_entries(ino, entries, &count) != 0) return -1;
    if (count != 0) return -1;

    if (in->direct[0]) vfs_free_block(in->direct[0]);

    char parent[MAX_PATH_LENGTH], name[MAX_FILENAME_LENGTH];
    if (split_parent_child(path, parent, name) != 0) return -1;
    uint32_t pino = 0;
    if (vfs_resolve_path(parent, &pino) != 0) return -1;
    if (dir_remove_child(pino, name) != 0) return -1;

    vfs_free_inode(ino);
    save_inode_table();
    save_superblock();
    return 0;
}

int vfs_list(const char *path, char *buf, uint32_t max_len) {
    if (!buf || max_len == 0) return -1;
    buf[0] = 0;

    uint32_t ino = 0;
    if (!path || str_len(path) == 0) path = ".";

    if (vfs_resolve_path(path, &ino) != 0) return -1;
    if (vfs.inodes[ino].type != INODE_TYPE_DIR) return -1;

    uint8_t list_buf[BLOCK_SIZE];
    dir_entry_t *entries = (dir_entry_t *)list_buf;
    uint32_t count = 0;
    if (dir_read_entries(ino, entries, &count) != 0) return -1;

    uint32_t out = 0;
    for (uint32_t i = 0; i < count; i++) {
        inode_t *child = &vfs.inodes[entries[i].inode_number];
        const char *suffix = (child->type == INODE_TYPE_DIR) ? "/" : "";
        uint32_t nlen = str_len(entries[i].name);
        uint32_t slen = str_len(suffix);

        if (out + nlen + slen + 2 >= max_len) break;

        for (uint32_t j = 0; j < nlen; j++) buf[out++] = entries[i].name[j];
        for (uint32_t j = 0; j < slen; j++) buf[out++] = suffix[j];
        buf[out++] = '\n';
    }

    buf[out] = 0;
    return 0;
}

int vfs_chdir(const char *path) {
    uint32_t ino = 0;
    if (vfs_resolve_path(path, &ino) != 0) return -1;
    if (vfs.inodes[ino].type != INODE_TYPE_DIR) return -1;

    vfs.cwd.inode_number = ino;

    if (path[0] == '/') {
        str_copy(vfs.cwd.path, path, MAX_PATH_LENGTH);
    } else if (str_eq(path, ".")) {
    } else if (str_eq(path, "..")) {
        if (!str_eq(vfs.cwd.path, "/")) {
            uint32_t len = str_len(vfs.cwd.path);
            while (len > 1 && vfs.cwd.path[len - 1] != '/') len--;
            if (len > 1) vfs.cwd.path[len - 1] = 0;
            else {
                vfs.cwd.path[0] = '/';
                vfs.cwd.path[1] = 0;
            }
        }
    } else {
        if (str_eq(vfs.cwd.path, "/")) {
            uint32_t l = str_len(vfs.cwd.path);
            if (l + str_len(path) + 1 < MAX_PATH_LENGTH) {
                for (uint32_t i = 0; path[i]; i++) vfs.cwd.path[l++] = path[i];
                vfs.cwd.path[l] = 0;
            }
        } else {
            uint32_t l = str_len(vfs.cwd.path);
            if (l + 1 + str_len(path) < MAX_PATH_LENGTH) {
                vfs.cwd.path[l++] = '/';
                for (uint32_t i = 0; path[i]; i++) vfs.cwd.path[l++] = path[i];
                vfs.cwd.path[l] = 0;
            }
        }
    }

    return 0;
}

void vfs_getcwd(char *buf, uint32_t max_len) {
    if (!buf || max_len == 0) return;
    str_copy(buf, vfs.cwd.path, max_len);
}

uint32_t vfs_get_ticks(void) {
    return timer_get_ticks();
}

void vfs_set_ticks(uint32_t ticks) {
    vfs.timer_ticks = ticks;
    (void)ticks;
}
