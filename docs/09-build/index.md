---
layout: default
title: build
---

# build

## prasyarat

```bash
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
```

## make targets

| target | deskripsi |
|--------|-----------|
| `make` | build kernel.bin + oasis.iso |
| `make clean` | hapus semua artifact |
| `make run` | boot di qemu |

## build detail

### compile flags

```
-m32 -nostdlib -fno-builtin -fno-stack-protector
-ffreestanding -fno-pie -fno-pic -Wall -Wextra
```

### assembly

nasm assemble dengan `-f elf32`.

### linking

```
ld -m elf_i386 -T src/boot/linker.ld -no-pie
```

entry point di `_start` (entry.asm), kernel di-load di 1mb.

### iso image

```
cp kernel.bin iso/boot/kernel
grub-mkrescue -o oasis.iso iso
```

## debugging

### qemu debug port

kernel output ke qemu debug port (0xE9):
```c
static void dbg_putc(char c) { outb(0xE9, (uint8_t)c); }
```

capture: `qemu -debugcon stdio`

### exception logging

semua exception tercatat ke dmesg:
```
dmesg    # view kernel log
```
