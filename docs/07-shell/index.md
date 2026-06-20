---
layout: default
title: shell
---

# shell

shell berjalan sebagai infinite loop di `kernel_main()`. pake `keyboard_getchar()` buat baca input.

## command

semua command diimplementasikan sebagai if-else chain:

```c
if (strcmp(input, "help") == 0) { ... }
else if (starts_with(input, "edit ")) { ... }
else if (starts_with(input, "cat ")) { ... }
else if (starts_with(input, "nasm ")) { ... }
else if (starts_with(input, "occ ")) { ... }
else if (starts_with(input, "user ")) { ... }
else { vga_print("perintah tidak dikenal\n"); }
```

### file operations

| command | fungsi |
|---------|--------|
| `ls [path]` | vfs_list() -> print color-coded |
| `cd <path>` | vfs_chdir() |
| `pwd` | vfs_getcwd() |
| `mkdir <p>` | vfs_mkdir() |
| `touch <p>` | vfs_create() |
| `rm <p>` | vfs_unlink() |
| `cat <p>` | vfs_open -> read loop -> print |
| `write <p> <t>` | vfs_open + vfs_write |
| `echo <text>` | vga_print() |
| `hexdump <p>` | read + hex print |

### development

| command | fungsi |
|---------|--------|
| `edit <p>` | editor_run() -> nano-like editor |
| `nasm <p>` | asm_run_file() -> assemble + run di ring 0 |
| `user <p>` | run_user_test() -> assemble + run di ring 3 |
| `occ <p>` | run_occ() -> compile c + assemble + run |

### system

| command | fungsi |
|---------|--------|
| `dmesg` | log_dump() |
| `syscall` | print 23 syscall |
| `uptime` | ticks / 100 = detik |
| `meminfo` | pmm info |
| `clear` | vga_clear() |
