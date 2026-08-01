---
layout: default
title: shell
---

# Shell

The kernel shell runs inside `kernel_main()` and combines filesystem operations, diagnostics, development tools, and network inspection. It is currently a VGA text-mode interface.

## Runtime loop

The shell polls Ethernet before reading keyboard input, so ARP and ICMP packets can be handled while the user is idle:

```c
while (1) {
    eth_dispatch();
    if (!keyboard_available()) {
        asm volatile("hlt");
        continue;
    }
    char c = keyboard_getchar();
    /* append input and dispatch complete lines */
}
```

## Network commands

### `pci`

Scan PCI bus 0 and print vendor/device IDs, classes, IRQ values, and discovered devices. Use this first when the NIC is not detected.

### `nicinfo`

Print RTL8139 I/O base, IRQ, MAC address, command state, interrupt state, current buffer pointer, and software RX position.

### `arp`

Print valid entries in the eight-entry IP-to-MAC cache. The cache is initially empty and is populated by received ARP packets.

### `ping <ip>`

Send four ICMP echo requests to a dotted IPv4 address. In the default QEMU setup, test the gateway with:

```text
ping 10.0.2.2
```

Each request uses a 64-byte ICMP message and waits while the kernel polls Ethernet frames. A cache miss first sends an ARP broadcast. Invalid addresses print `ping: invalid IP`; missing ARP or transmit support prints a network error.

## Filesystem and development commands

The remaining commands include `ls`, `cd`, `pwd`, `mkdir`, `touch`, `rm`, `rmdir`, `cat`, `write`, `append`, `hexdump`, `edit`, `nasm`, `user`, `lex`, `parse`, and `occ`. Use `help` or `help more` inside OaSis for the complete current list.

## Diagnostics

- `dmesg`: dump the circular kernel log.
- `uptime`: print PIT tick-based uptime.
- `meminfo`: print physical memory usage.
- `taskinfo`: print task state.
- `syscall`: print the system call table.

The [build and testing guide](../09-build/testing/) contains a complete networking smoke test.

### Previous shell implementation notes

The sections below retain the detailed filesystem and application command notes.


shell runs as an infinite loop in `kernel_main()` (`src/kernel/core/kernel.c`).

## Loop main

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
 // ... all commands ...
 else if (index != 0) {
 vga_print("unknown command: ");
 vga_print(input);
 vga_print("\n");
 }

 index = 0;
 // print prompt lagi
 vga_print("oasis"); vga_putc('(');
 VFS_getcwd(cwd_buf, sizeof(cwd_buf));
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

## Command reference

### Help

print the command list. if `help more` show all commands, including less common commands.

### Ls [path]

list directory. parse output from `VFS_list()`:
- if type 'd' (directory): color kuning (VGA_color_YELLOW)
- if type 'f' (file): color putih

output from VFS_list format: `d nama_dir\nf nama_file\n`

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

### Cd <path>

call `VFS_chdir(arg)`. if return != 0, print "cd: failed".

### Pwd

call `VFS_getcwd(pathbuf, sizeof(pathbuf))`, print the result.

### Mkdir <path>

call `VFS_mkdir(arg)`.

### Touch <path>

call `VFS_create(arg)`.

### Rm <path>

call `VFS_unlink(arg)`.

### Rmdir <path>

call `VFS_rmdir(arg)`.

### Cat <path>

```c
int fd = VFS_open(arg, VFS_O_READ);
if (fd < 0) { vga_print("cat: open failed\n"); }
else {
 char rbuf[128]; int n;
 while ((n = VFS_read(fd, rbuf, sizeof(rbuf) - 1)) > 0) {
 rbuf[n] = 0;
 vga_print(rbuf);
 }
 vga_print("\n");
 VFS_close(fd);
}
```

### Write <path> <text>

```c
int fd = VFS_open(arg, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
if (fd >= 0) {
 VFS_write(fd, text, strlen(text));
 VFS_close(fd);
}
```

### Append <path> <text>

the same as write, but uses VFS_O_APPEND.

### Echo <text>

print text + newline.

### Hexdump <path>

read file, print tiap byte as hex.

### Edit <path>

call `editor_run(arg)`.

### Nasm <path>

call `asm_run_file(arg)`.

### User <path>

call `run_user_test(arg)`.

### Occ <path>

call `run_occ(arg)`.

### Dmesg

call `log_dump()`.

### Syscall

print tabel syscall.

### Uptime

`timer_get_ticks() / 100` = detik sejak boot.

### Meminfo

print `PMM_get_free_pages()` + total memory.

### Taskinfo

print all task + status + stack + eip.
