---
layout: default
title: shell
---

# shell

shell berjalan sebagai infinite loop di `kernel_main()` (`src/kernel/core/kernel.c`).

## loop utama

```c
while (1) {
 char c = keyboard_getchar();

 if (c == '\n') {
 input[index] = 0; // null-terminate
 vga_putc('\n');
 vga_refresh_cursor();

 // parse command
 if (strcmp(input, "help") == 0) { ... }
 else if (starts_with(input, "edit ")) { ... }
 else if (starts_with(input, "cat ")) { ... }
 // ... semua command ...
 else if (index != 0) {
 vga_print("perintah tidak dikenal: ");
 vga_print(input);
 vga_print("\n");
 }

 index = 0;
 // print prompt lagi
 vga_print("oasis"); vga_putc('(');
 vfs_getcwd(cwd_buf, sizeof(cwd_buf));
 vga_print(cwd_buf); vga_putc(')'); vga_print("> ");
 vga_refresh_cursor();
 }
 else if (c == '\b') {
 if (index > 0) { index--; vga_putc('\b'); vga_refresh_cursor(); }
 }
 else {
 if (index < INPUT_MAX - 1) { input[index++] = c; vga_putc(c); vga_refresh_cursor(); }
 }
}
```

INTERNAL_MAX = 256.

## command reference

### help

print daftar command. kalau `help more` tampilkan semua command termasuk yang jarang dipakai.

### ls [path]

list directory. parse output dari `vfs_list()`:
- kalau type 'd' (directory): warna kuning (VGA_COLOR_YELLOW)
- kalau type 'f' (file): warna putih

output dari vfs_list format: `d nama_dir\nf nama_file\n`

```c
if (out[j] == 'd') {
 vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
 j += 2; // skip "d "
 while (out[j] != 0 && out[j] != '\n') vga_putc(out[j++]);
 if (out[j] == '\n') j++;
 vga_set_color(15, VGA_COLOR_BLACK);
 vga_print(" "); // spacing antar item
}
```

### cd <path>

panggil `vfs_chdir(arg)`. kalau return != 0, print "cd: failed".

### pwd

panggil `vfs_getcwd(pathbuf, sizeof(pathbuf))`, print hasilnya.

### mkdir <path>

panggil `vfs_mkdir(arg)`.

### touch <path>

panggil `vfs_create(arg)`.

### rm <path>

panggil `vfs_unlink(arg)`.

### rmdir <path>

panggil `vfs_rmdir(arg)`.

### cat <path>

```c
int fd = vfs_open(arg, VFS_O_READ);
if (fd < 0) { vga_print("cat: open failed\n"); }
else {
 char rbuf[128]; int n;
 while ((n = vfs_read(fd, rbuf, sizeof(rbuf) - 1)) > 0) {
 rbuf[n] = 0;
 vga_print(rbuf);
 }
 vga_print("\n");
 vfs_close(fd);
}
```

### write <path> <text>

```c
int fd = vfs_open(arg, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
if (fd >= 0) {
 vfs_write(fd, text, strlen(text));
 vfs_close(fd);
}
```

### append <path> <text>

sama seperti write tapi pakai VFS_O_APPEND.

### echo <text>

print teks + newline.

### hexdump <path>

read file, print tiap byte sebagai hex.

### edit <path>

panggil `editor_run(arg)`.

### nasm <path>

panggil `asm_run_file(arg)`.

### user <path>

panggil `run_user_test(arg)`.

### occ <path>

panggil `run_occ(arg)`.

### dmesg

panggil `log_dump()`.

### syscall

print tabel syscall.

### uptime

`timer_get_ticks() / 100` = detik sejak boot.

### meminfo

print `pmm_get_free_pages()` + total memory.

### taskinfo

print semua task + status + stack + eip.
